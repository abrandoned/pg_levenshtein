Postgres Levenshtein UDF
====

Implementation of levenshtein edit distance algorithm to be used in mysql as a UDF.

Using scons to build the "libpg_levenshtein.so" file, but also included a Ruby FFI
version and a rakefile with specs in it for testing. (Ruby specs currently are not running
in pg version, this was thrown together quickly)

Move the libpg_levenshtein.so file to your postgres plugins directory and install with:
  
  "CREATE FUNCTION levenshtein(text, text, integer) RETURNS integer AS
    'libpg_levenshtein', 'levenshtein_threshold' LANGUAGE C STRICT;"

  "CREATE FUNCTION levenshtein(text, text) RETURNS integer AS
    'libpg_levenshtein', 'levenshtein' LANGUAGE C STRICT;"

Postgres considered different signatures to be different functions, therefore you must
install both to use the same as the MySql UDF.


Usage
====
  SQL: 
  
  "select * from table where levenshtein(column1, column2) < 10"
  
  "select * from table where levenshtein(column1, column2, 10) < 10"
  
  The 3 parameter signature adds a threshold to the comparison and can significantly
  increase the execution speed.  (If it can exit upon exceeding the threshold or
  determine that it does not need to execute because of the length differences then
  we get the execution cycles back and everybody wins).
  
Performance
====
  While there are certainly things that can be done to increase the speed, this
  implementation of levenshtein is quite fast and efficient.  The space required is only
  2*min(length(A), length(B)).
  
  rough benchmark: A full table scan on the first traunch of the google N-gram data
  (which contains ~48million words) takes approx 56 sec on my machine.  Adding a naive
  levenshtein distance calculation to each word only results in approx 10 sec change in
  execution speed.  (ie - it is pretty fast)
