#include "postgres.h"
#include "fmgr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef TRUE
#define TRUE 1  
#define FALSE 0
#endif

#ifdef RSPEC
#define ALLOC malloc
#define FREE free
#else
#define ALLOC palloc
#define FREE pfree
#endif

/* Used to swap calc and work grids while looping */
#define LV_SWAP(t, a, b) { t = a; a = b; b = t;}
#define LV_MIN(a, b, c) ((a <= b) ? ((a <= c) ? a : c) : ((b <= c) ? b : c));

/* Added to enforce Postgres major version compat */
#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/* using a structure that will be filled on init with the variables that will makeup the distance constraints */
typedef struct{
  char* a; 
  char* b; 
  int a_length;
  int b_length;
  int32 maximum_allowable_distance;
} LevenConstraints;

int32 levenshtein_intern(LevenConstraints* leven)
{
  /* Check the minimum distance to keep from calculating anything when it isn't really needed
   * Efficient use of the levenshtein call includes a maximum_allowable_distance (or distance threshold) */
  if ((leven->b_length - leven->a_length) > leven->maximum_allowable_distance || (leven->a_length == 0 || leven->b_length == 0)) {
    return (int32)leven->b_length;
  }

  /* no reason to run levenshtein when equal */
  if (leven->a_length == leven->b_length && strcmp(leven->a, leven->b) == 0) {
    return (int32)0;
  }

  leven->a_length++;
  leven->b_length++;

  int x, *grid_odd, *grid_even, i, j, cost, row_min, distance, *work_grid, *calc_grid, *tmp;
  unsigned int broke_max = FALSE;

  grid_even = ALLOC(sizeof(int) * (leven->a_length));
  grid_odd = ALLOC(sizeof(int) * (leven->a_length));

  if(grid_even == NULL || grid_odd == NULL) {
    return (int32)9999;   /* error occured - cannot allocate memory */
  }  

  work_grid = grid_odd;
  calc_grid = grid_even;

  for(x = 0; x < leven->a_length; x++)
    grid_even[x] = x;

  for(i = 1; i < leven->b_length; i++) {     
    row_min = work_grid[0] = calc_grid[0] + 1;

    for(j = 1; j < leven->a_length; j++) {
      cost = (leven->a[j-1] == leven->b[i-1]) ? 0 : 1;
      work_grid[j] = LV_MIN(calc_grid[j]+1, work_grid[j-1]+1, calc_grid[j-1] + cost);
      row_min = (work_grid[j] < row_min) ? work_grid[j] : row_min;            
    }

    if(row_min > leven->maximum_allowable_distance) { 
      broke_max = TRUE; 
      break;
    }

    LV_SWAP(tmp, work_grid, calc_grid);
  }

  distance = (broke_max == TRUE) ? (leven->b_length - 1) : calc_grid[leven->a_length-1];  

  FREE(grid_odd);
  FREE(grid_even);  

  return (int32) distance;
}

int32 levenshtein_extern(char* a, char* b, int32 max_distance)
{
  int a_len = (a == NULL) ? 0 : strlen(a);
  int b_len = (b == NULL) ? 0 : strlen(b);

  LevenConstraints* leven = malloc(sizeof(LevenConstraints));
  leven->a = (a_len > b_len) ? b : a;
  leven->b = (a_len > b_len) ? a : b;
  leven->a_length = strlen(leven->a);
  leven->b_length = strlen(leven->b);

  int32 distance = leven->b_length;

  if(max_distance < 0) {
    max_distance = (a_len > b_len) ? a_len : b_len;;
  }

  leven->maximum_allowable_distance = max_distance;

  distance = levenshtein_intern(leven);  
  FREE(leven);

  return distance;
}

#ifdef PG_FUNCTION_INFO_V1
PG_FUNCTION_INFO_V1(levenshtein);
#endif

/* Function signature without threshold */
Datum
levenshtein(PG_FUNCTION_ARGS)
{
  int32 distance = 9999;

  text *first_argument;
  text *second_argument;
  text *shortest;
  text *longest;
  int first_length;
  int second_length;
  int shortest_length;
  int longest_length;

  if (PG_ARGISNULL(0) || PG_ARGISNULL(1)) {
    PG_RETURN_NULL();
  }

  first_argument = PG_GETARG_TEXT_P(0);
  second_argument = PG_GETARG_TEXT_P(1);
  first_length = VARSIZE(first_argument) - VARHDRSZ;
  second_length = VARSIZE(second_argument) - VARHDRSZ;
  shortest_length = (first_length < second_length) ? first_length : second_length;
  longest_length = (first_length < second_length) ? second_length : first_length;
  shortest = (first_length < second_length) ? first_argument : second_argument;
  longest = (first_length < second_length) ? second_argument : first_argument;

  LevenConstraints* leven = ALLOC(sizeof(LevenConstraints));

  char* h_bufa = ALLOC((shortest_length+1) * sizeof(char));
  char* h_bufb = ALLOC((longest_length+1) * sizeof(char));
  memcpy(h_bufa, VARDATA_ANY(shortest), shortest_length);
  memcpy(h_bufb, VARDATA_ANY(longest), longest_length);    
  leven->a = h_bufa;
  leven->b = h_bufb;

  leven->a_length = shortest_length;
  leven->b_length = longest_length;
  leven->maximum_allowable_distance = longest_length;

  distance = levenshtein_intern(leven);

  FREE(leven->a);
  FREE(leven->b);
  FREE(leven);

  PG_RETURN_INT32(distance);
}

#ifdef PG_FUNCTION_INFO_V1
PG_FUNCTION_INFO_V1(levenshtein_threshold);
#endif

/* Function signature with threshold */
Datum
levenshtein_threshold(PG_FUNCTION_ARGS)
{
  int32 distance = 9999;

  text *first_argument;
  text *second_argument;
  text *shortest;
  text *longest;
  int first_length;
  int second_length;
  int shortest_length;
  int longest_length;

  if (PG_ARGISNULL(0) || PG_ARGISNULL(1) || PG_ARGISNULL(2)) {
    PG_RETURN_NULL();
  }

  first_argument = PG_GETARG_TEXT_P(0);
  second_argument = PG_GETARG_TEXT_P(1);
  first_length = VARSIZE(first_argument) - VARHDRSZ;
  second_length = VARSIZE(second_argument) - VARHDRSZ;
  shortest_length = (first_length < second_length) ? first_length : second_length;
  longest_length = (first_length < second_length) ? second_length : first_length;
  shortest = (first_length < second_length) ? first_argument : second_argument;
  longest = (first_length < second_length) ? second_argument : first_argument;

  LevenConstraints* leven = ALLOC(sizeof(LevenConstraints));

  char* h_bufa = ALLOC((shortest_length+1) * sizeof(char));
  char* h_bufb = ALLOC((longest_length+1) * sizeof(char));
  memcpy(h_bufa, VARDATA_ANY(shortest), shortest_length);
  memcpy(h_bufb, VARDATA_ANY(longest), longest_length);    
  leven->a = h_bufa;
  leven->b = h_bufb;

  leven->a_length = shortest_length;
  leven->b_length = longest_length;
  leven->maximum_allowable_distance = PG_GETARG_INT32(2); 

  distance = levenshtein_intern(leven);

  FREE(leven->a);
  FREE(leven->b);
  FREE(leven);

  PG_RETURN_INT32(distance);
}
