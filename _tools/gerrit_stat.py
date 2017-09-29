# Returns usage statistics for the given Gerrit repository
# Usage: python gerrit_stat.py -s server -p port -r repository -l limit

import argparse
import json
import operator
import os
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument(
  "-s", "--server", dest="server",
  required=True,
  help="Gerrit server")
parser.add_argument(
  "-p", "--port", dest="port",
  required=True,
  help="Port on Gerrit server")
parser.add_argument(
  "-r", "--repo", dest="repo",
  required=True,
  help="Repository to query stat")
parser.add_argument(
  "-l", "--limit", dest="limit",
  required=False, default=500,
  help="Limit on number of changes to query and average")

args = parser.parse_args()

status = ['open', 'merged', 'abandoned']
json_stats = {}
stats = {}

result = os.popen('ssh -p {port} {server} gerrit ls-projects -p {repo}'.format(
   port=args.port, server=args.server, repo=args.repo)).read()

found = False
if len(result.splitlines()) == 1:
  for line in result.splitlines():
    if line == args.repo:
      found = True

if found == False:
  print('error: no such project: ' + args.repo)
  exit()

def is_stat_valid(stat):
   stat_keys = ['patchSets', 'createdOn', 'lastUpdated', 'owner']
   for key in stat_keys:
     if not (key in stat):
       return False
   if not ('name' in stat['owner']):
     return False
   return True

for s in status:
  print('info: query data for ' + s + ' changes (limit=' + str(args.limit) + ')...')
  result = os.popen('ssh -p {port} {server} gerrit query --format=JSON --patch-sets project:{repo} status:{status} limit:{limit}'.format(
    port=args.port, server=args.server, repo=args.repo, status=s, limit=args.limit)).read()

  json_stats[s] = []
  for line in result.splitlines():
    json_stats[s].append(json.loads(line))

  stats[s] = {
    'changes': 0,
    'max_patch_sets': 0,
    'avg_patch_sets': 0.0,
    'max_live_circle_days': 0.0,
    'avg_live_circle_days': 0.0,
    'authors': {}
    }

  for stat in json_stats[s]:
    if 'rowCount' in stat:
      if stats[s]['changes'] != int(stat['rowCount']):
        print('error: broken statistics output')
    else:
      if is_stat_valid(stat) == False:
        print('error: broken statistics entry')
        continue

      stats[s]['changes'] += 1

      # If repository was damaged and restored from the backup, some changes
      # may not have current patch set available.
      if stats[s]['max_patch_sets'] < len(stat['patchSets']):
        stats[s]['max_patch_sets'] = len(stat['patchSets'])
      stats[s]['avg_patch_sets'] += len(stat['patchSets'])

      live_circle = int(stat['lastUpdated']) - int(stat['createdOn'])
      if stats[s]['max_live_circle_days'] < live_circle:
         stats[s]['max_live_circle_days'] = live_circle
      stats[s]['avg_live_circle_days'] += float(live_circle)

      if stat['owner']['name'] in stats[s]['authors']:
        stats[s]['authors'][stat['owner']['name']] += 1
      else:
        stats[s]['authors'][stat['owner']['name']] = 1

  stats[s]['max_live_circle_days'] /= float(60*60*24)
  stats[s]['avg_live_circle_days'] /= float(60*60*24)

  if stats[s]['changes'] != 0:
    stats[s]['avg_patch_sets'] /= stats[s]['changes']
    stats[s]['avg_live_circle_days'] /= stats[s]['changes']

for stat in stats:
  for metric in stats[stat]:
    if metric != 'authors':
      print(stat + '.' + metric + '=' + str(stats[stat][metric]))
    else:
      print(stat + '.' + metric + '= [')
      sorted_authors = sorted(stats[stat][metric].items(),
        key=operator.itemgetter(1), reverse=True)
      print('  # <author>: <num_changes>')
      for author, changes in sorted_authors:
        print('  ' + author + ': ' + str(changes))
      print(']')
