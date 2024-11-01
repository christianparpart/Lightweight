
/* Find 10 reviews with the highest number of reviews */
select user_id, COUNT(movie_id) from ratings
group by user_id
order by COUNT(movie_id) desc
limit 10;
