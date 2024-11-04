#!/bin/sh
#
#

# Download https://www.kaggle.com/datasets/clementmsika/mubi-sqlite-database-for-movie-lovers
# and put it in the same directory as this script

curl -L -o mubi-sqlite-database-for-movie-lovers.zip https://www.kaggle.com/api/v1/datasets/download/clementmsika/mubi-sqlite-database-for-movie-lovers

unzip mubi-sqlite-database-for-movie-lovers.zip
rm mubi-sqlite-database-for-movie-lovers.zip
rm *.csv
