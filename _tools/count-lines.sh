# Computes c++ code lines count recursively from the current dir
set -e

echo "all, production, test lines, test count:"

prod_lines=$(find . -type f -not -path "./unittests/*" -not -path "./codec2/*" \( -name *.cpp -or -name *.h \) | xargs cat | wc -l)

test_lines=$(find ./unittests \( -name *.cpp -or -name *.h \) | xargs cat | wc -l)

lines=$(find . -not -path "./codec2/*" \( -name *.cpp -or -name *.h \) | xargs cat | wc -l)

test_count=$(grep -r --include=*.cpp "TEST(" ./unittests/ | wc -l)

echo $lines';'$prod_lines';'$test_lines';'$test_count
