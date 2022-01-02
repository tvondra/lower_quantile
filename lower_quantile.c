/*
 * lower_quantile.c - Lower quantile aggregate function
 *
 * Copyright (C) Tomas Vondra, 2011
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>

#include "postgres.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/numeric.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"
#include "nodes/execnodes.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif


#define GET_AGG_CONTEXT(fname, fcinfo, aggcontext)  \
	if (! AggCheckCallContext(fcinfo, &aggcontext)) {   \
		elog(ERROR, "%s called in non-aggregate context", fname);  \
	}

#define CHECK_AGG_CONTEXT(fname, fcinfo)  \
	if (! AggCheckCallContext(fcinfo, NULL)) {   \
		elog(ERROR, "%s called in non-aggregate context", fname);  \
	}

/*
 * Structures used to keep the data - the 'elements' array is extended
 * on the fly if needed.
 */
typedef struct quantile_state
{
	int	nquantiles;		/* size of the quantiles array */
	int	maxelements;	/* size of the elements array */
	int	nelements;		/* number of elements */

	/* arrays of elements and requested quantiles */
	double *quantiles;
	void   *elements;
} quantile_state;

#define	QUANTILE_MIN_ELEMENTS	4

/* comparators, used for qsort */

static int  double_comparator(const void *a, const void *b);

/* parse the quantiles array */
static double *
array_to_double(FunctionCallInfo fcinfo, ArrayType *v, int * len);

static double *
numeric_array_to_double(FunctionCallInfo fcinfo, ArrayType *v, int * len);

static Datum
double_to_array(FunctionCallInfo fcinfo, double * d, int len);

static void
check_quantiles(int nquantiles, double * quantiles);

/* prototypes */
PG_FUNCTION_INFO_V1(quantile_append_double_array);
PG_FUNCTION_INFO_V1(quantile_append_double_array_numeric);
PG_FUNCTION_INFO_V1(quantile_append_double);
PG_FUNCTION_INFO_V1(quantile_append_double_numeric);

PG_FUNCTION_INFO_V1(quantile_double_array);
PG_FUNCTION_INFO_V1(quantile_double);

Datum quantile_append_double_array(PG_FUNCTION_ARGS);
Datum quantile_append_double_array_numeric(PG_FUNCTION_ARGS);

Datum quantile_append_double(PG_FUNCTION_ARGS);
Datum quantile_append_double_numeric(PG_FUNCTION_ARGS);

Datum quantile_double_array(PG_FUNCTION_ARGS);
Datum quantile_double(PG_FUNCTION_ARGS);

static void
AssertCheckQuantileState(quantile_state *state)
{
#ifdef USE_ASSERT_CHECKING
	Assert(state->nquantiles >= 1);

	Assert(state->nelements >= 0);
	Assert(state->nelements <= state->maxelements);
#endif
}

/*
 * The memory consumption might be a problem, as all the values are
 * kept in the memory - for example 1.000.000 of 8-byte values (bigint)
 * requires about 8MB of memory.
 */

Datum
quantile_append_double(PG_FUNCTION_ARGS)
{
	quantile_state *state;

	MemoryContext	oldcontext;
	MemoryContext	aggcontext;

	double		   *elements;

	/* OK, we do want to skip NULL values altogether */
	if (PG_ARGISNULL(1))
	{
		if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
		else
			/* if there already is a state accumulated, don't forget it */
			PG_RETURN_DATUM(PG_GETARG_DATUM(0));
	}

	GET_AGG_CONTEXT("lower_quantile_append_double", fcinfo, aggcontext);

	oldcontext = MemoryContextSwitchTo(aggcontext);

	if (PG_ARGISNULL(0))
	{
		state = (quantile_state *) palloc(sizeof(quantile_state));
		state->elements  = palloc(QUANTILE_MIN_ELEMENTS * sizeof(double));
		state->maxelements = QUANTILE_MIN_ELEMENTS;
		state->nelements = 0;

		state->quantiles = (double *) palloc(sizeof(double));
		state->quantiles[0] = PG_GETARG_FLOAT8(2);
		state->nquantiles = 1;

		check_quantiles(state->nquantiles, state->quantiles);
	}
	else
		state = (quantile_state *) PG_GETARG_POINTER(0);

	AssertCheckQuantileState(state);

	/* we can be sure the value is not null (see the check above) */
	if (state->nelements == state->maxelements)
	{
		state->maxelements *= 2;
		state->elements = repalloc(state->elements,
								   sizeof(double) * state->maxelements);
	}

	Assert(state->nelements < state->maxelements);

	/* make sure to cast the array to (double *) before updating it */
	elements = (double *) state->elements;
	elements[state->nelements++] = PG_GETARG_FLOAT8(1);

	MemoryContextSwitchTo(oldcontext);

	PG_RETURN_POINTER(state);
}

Datum
quantile_append_double_numeric(PG_FUNCTION_ARGS)
{
	quantile_state *state;

	MemoryContext	oldcontext;
	MemoryContext	aggcontext;

	double		   *elements;

	/* OK, we do want to skip NULL values altogether */
	if (PG_ARGISNULL(1))
	{
		if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
		else
			/* if there already is a state accumulated, don't forget it */
			PG_RETURN_DATUM(PG_GETARG_DATUM(0));
	}

	GET_AGG_CONTEXT("lower_quantile_append_double", fcinfo, aggcontext);

	oldcontext = MemoryContextSwitchTo(aggcontext);

	if (PG_ARGISNULL(0))
	{
		state = (quantile_state *) palloc(sizeof(quantile_state));
		state->elements  = palloc(QUANTILE_MIN_ELEMENTS * sizeof(double));
		state->maxelements = QUANTILE_MIN_ELEMENTS;
		state->nelements = 0;

		state->quantiles = (double *) palloc(sizeof(double));

		state->quantiles[0]
			= (float8) DatumGetFloat8(DirectFunctionCall1(numeric_float8_no_overflow,
														  NumericGetDatum(PG_GETARG_NUMERIC(2))));

		state->nquantiles = 1;

		check_quantiles(state->nquantiles, state->quantiles);
	}
	else
		state = (quantile_state *) PG_GETARG_POINTER(0);

	AssertCheckQuantileState(state);

	/* we can be sure the value is not null (see the check above) */
	if (state->nelements == state->maxelements)
	{
		state->maxelements *= 2;
		state->elements = repalloc(state->elements,
								   sizeof(double) * state->maxelements);
	}

	Assert(state->nelements < state->maxelements);

	/* make sure to cast the array to (double *) before updating it */
	elements = (double *) state->elements;
	elements[state->nelements++] = PG_GETARG_FLOAT8(1);

	MemoryContextSwitchTo(oldcontext);

	PG_RETURN_POINTER(state);
}

Datum
quantile_append_double_array(PG_FUNCTION_ARGS)
{
	quantile_state *state;

	MemoryContext	oldcontext;
	MemoryContext	aggcontext;

	double		   *elements;
	ArrayType	   *quantiles;

	/* OK, we do want to skip NULL values altogether */
	if (PG_ARGISNULL(1))
	{
		if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
		else
			/* if there already is a state accumulated, don't forget it */
			PG_RETURN_DATUM(PG_GETARG_DATUM(0));
	}

	quantiles = PG_GETARG_ARRAYTYPE_P(2);

	GET_AGG_CONTEXT("lower_quantile_append_double_array", fcinfo, aggcontext);

	oldcontext = MemoryContextSwitchTo(aggcontext);

	if (PG_ARGISNULL(0))
	{
		state = (quantile_state *) palloc(sizeof(quantile_state));
		state->elements  = palloc(QUANTILE_MIN_ELEMENTS * sizeof(double));
		state->maxelements = QUANTILE_MIN_ELEMENTS;
		state->nelements = 0;

		/* read the array of quantiles */
		state->quantiles = array_to_double(fcinfo, quantiles,
										   &state->nquantiles);

		check_quantiles(state->nquantiles, state->quantiles);
	}
	else
		state = (quantile_state *) PG_GETARG_POINTER(0);

	AssertCheckQuantileState(state);

	/* we can be sure the value is not null (see the check above) */
	if (state->nelements == state->maxelements)
	{
		state->maxelements *= 2;
		state->elements = repalloc(state->elements,
								   sizeof(double) * state->maxelements);
	}

	Assert(state->nelements < state->maxelements);

	elements = (double *) state->elements;
	elements[state->nelements++] = PG_GETARG_FLOAT8(1);

	MemoryContextSwitchTo(oldcontext);

	PG_RETURN_POINTER(state);
}

Datum
quantile_append_double_array_numeric(PG_FUNCTION_ARGS)
{
	quantile_state *state;

	MemoryContext	oldcontext;
	MemoryContext	aggcontext;

	double		   *elements;
	ArrayType	   *quantiles;

	/* OK, we do want to skip NULL values altogether */
	if (PG_ARGISNULL(1))
	{
		if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
		else
			/* if there already is a state accumulated, don't forget it */
			PG_RETURN_DATUM(PG_GETARG_DATUM(0));
	}

	quantiles = PG_GETARG_ARRAYTYPE_P(2);

	GET_AGG_CONTEXT("lower_quantile_append_double_array", fcinfo, aggcontext);

	oldcontext = MemoryContextSwitchTo(aggcontext);

	if (PG_ARGISNULL(0))
	{
		state = (quantile_state *) palloc(sizeof(quantile_state));
		state->elements  = palloc(QUANTILE_MIN_ELEMENTS * sizeof(double));
		state->maxelements = QUANTILE_MIN_ELEMENTS;
		state->nelements = 0;

		/* read the array of quantiles */
		state->quantiles = numeric_array_to_double(fcinfo, quantiles,
										   &state->nquantiles);

		check_quantiles(state->nquantiles, state->quantiles);
	}
	else
		state = (quantile_state *) PG_GETARG_POINTER(0);

	AssertCheckQuantileState(state);

	/* we can be sure the value is not null (see the check above) */
	if (state->nelements == state->maxelements)
	{
		state->maxelements *= 2;
		state->elements = repalloc(state->elements,
								   sizeof(double) * state->maxelements);
	}

	Assert(state->nelements < state->maxelements);

	elements = (double *) state->elements;
	elements[state->nelements++] = PG_GETARG_FLOAT8(1);

	MemoryContextSwitchTo(oldcontext);

	PG_RETURN_POINTER(state);
}

Datum
quantile_double(PG_FUNCTION_ARGS)
{
	int				idx = 0;
	quantile_state *state;
	double		   *elements;

	CHECK_AGG_CONTEXT("lower_quantile_double", fcinfo);

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	state = (quantile_state *) PG_GETARG_POINTER(0);
	elements = (double *) state->elements;

	qsort(state->elements, state->nelements, sizeof(double), &double_comparator);

	if (state->quantiles[0] > 0)
		idx = (int) floor((state->nelements - 1) * state->quantiles[0]);

	PG_RETURN_FLOAT8(elements[idx]);
}

Datum
quantile_double_array(PG_FUNCTION_ARGS)
{
	int				i;
	double		   *result;
	quantile_state *state;
	double		   *elements;

	CHECK_AGG_CONTEXT("lower_quantile_double_array", fcinfo);

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	state = (quantile_state *) PG_GETARG_POINTER(0);

	result = palloc(state->nquantiles * sizeof(double));
	elements = (double *) state->elements;

	qsort(state->elements, state->nelements, sizeof(double), &double_comparator);

	for (i = 0; i < state->nquantiles; i++)
	{
		int	idx = 0;

		if (state->quantiles[i] > 0)
			idx = (int) floor((state->nelements - 1.0) * state->quantiles[i]);

		result[i] = elements[idx];
	}

	return double_to_array(fcinfo, result, state->nquantiles);
}


/* Comparators for the qsort() calls. */

static int
double_comparator(const void *a, const void *b)
{
	double af = (* (double*) a);
	double bf = (* (double*) b);
	return (af > bf) - (af < bf);
}

/*
 * Reading quantiles from an input array, based mostly on
 * array_to_text_internal (it's a modified copy). This expects
 * to receive a single-dimensional float8 array as input, fails
 * otherwise.
 */
static double *
array_to_double(FunctionCallInfo fcinfo, ArrayType *array, int *arraylen)
{
	int			i;
	Datum	   *keys;
	int			nkeys;

	/* info for deconstructing the array */
	Oid			element_type;
	int			typlen;
	bool		typbyval;
	char		typalign;

	ArrayMetaState *my_extra;

	/* result */
	double	   *result;

	element_type = ARR_ELEMTYPE(array);

	if (element_type != FLOAT8OID)
		elog(ERROR, "array expected to be double precision[]");

	/*
	 * We arrange to look up info about element type, including its output
	 * conversion proc, only once per series of calls, assuming the element
	 * type doesn't change underneath us.
	 */
	my_extra = (ArrayMetaState *) fcinfo->flinfo->fn_extra;
	if (my_extra == NULL)
	{
		fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt,
													  sizeof(ArrayMetaState));
		my_extra = (ArrayMetaState *) fcinfo->flinfo->fn_extra;
		my_extra->element_type = ~element_type;
	}

	/*
	 * Get info about element type, including its output conversion proc, if
	 * we haven't done that already.
	 */
	if (my_extra->element_type != element_type)
	{
		get_type_io_data(element_type, IOFunc_output,
						 &my_extra->typlen, &my_extra->typbyval,
						 &my_extra->typalign, &my_extra->typdelim,
						 &my_extra->typioparam, &my_extra->typiofunc);
		fmgr_info_cxt(my_extra->typiofunc, &my_extra->proc,
					  fcinfo->flinfo->fn_mcxt);
		my_extra->element_type = element_type;
	}

	typlen = my_extra->typlen;
	typbyval = my_extra->typbyval;
	typalign = my_extra->typalign;

	/* Extract data from array of int16 */
	deconstruct_array(array, FLOAT8OID, typlen, typbyval, typalign,
					  &keys, NULL, &nkeys);

	result = (double *) palloc(sizeof(double) * nkeys);

	for (i = 0; i < nkeys; i++)
		result[i] = DatumGetFloat8(keys[i]);

	*arraylen = nkeys;

	return result;
}

static double *
numeric_array_to_double(FunctionCallInfo fcinfo, ArrayType *array, int *arraylen)
{
	int			i;
	Datum	   *keys;
	int			nkeys;

	/* info for deconstructing the array */
	Oid			element_type;
	int			typlen;
	bool		typbyval;
	char		typalign;

	ArrayMetaState *my_extra;

	/* result */
	double	   *result;

	element_type = ARR_ELEMTYPE(array);

	if (element_type != NUMERICOID)
		elog(ERROR, "array expected to be double precision[]");

	/*
	 * We arrange to look up info about element type, including its output
	 * conversion proc, only once per series of calls, assuming the element
	 * type doesn't change underneath us.
	 */
	my_extra = (ArrayMetaState *) fcinfo->flinfo->fn_extra;
	if (my_extra == NULL)
	{
		fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt,
													  sizeof(ArrayMetaState));
		my_extra = (ArrayMetaState *) fcinfo->flinfo->fn_extra;
		my_extra->element_type = ~element_type;
	}

	/*
	 * Get info about element type, including its output conversion proc, if
	 * we haven't done that already.
	 */
	if (my_extra->element_type != element_type)
	{
		get_type_io_data(element_type, IOFunc_output,
						 &my_extra->typlen, &my_extra->typbyval,
						 &my_extra->typalign, &my_extra->typdelim,
						 &my_extra->typioparam, &my_extra->typiofunc);
		fmgr_info_cxt(my_extra->typiofunc, &my_extra->proc,
					  fcinfo->flinfo->fn_mcxt);
		my_extra->element_type = element_type;
	}

	typlen = my_extra->typlen;
	typbyval = my_extra->typbyval;
	typalign = my_extra->typalign;

	/* Extract data from array of numeric */
	deconstruct_array(array, NUMERICOID, typlen, typbyval, typalign,
					  &keys, NULL, &nkeys);

	result = (double *) palloc(sizeof(double) * nkeys);

	for (i = 0; i < nkeys; i++)
		result[i] = (float8) DatumGetFloat8(DirectFunctionCall1(numeric_float8_no_overflow,
											NumericGetDatum(keys[i])));

	*arraylen = nkeys;

	return result;
}

/*
 * Helper functions used to prepare the resulting array (when there's
 * an array of quantiles).
 */
static Datum
double_to_array(FunctionCallInfo fcinfo, double * d, int len)
{
	ArrayBuildState *astate = NULL;
	int		 i;

	for (i = 0; i < len; i++)
	{
		/* stash away this field */
		astate = accumArrayResult(astate,
								  Float8GetDatum(d[i]),
								  false,
								  FLOAT8OID,
								  CurrentMemoryContext);
	}

	PG_RETURN_ARRAYTYPE_P(makeArrayResult(astate,
										  CurrentMemoryContext));
}

static void
check_quantiles(int nquantiles, double * quantiles)
{
	int i = 0;
	for (i = 0; i < nquantiles; i++)
		if (quantiles[i] < 0 || quantiles[i] > 1)
			elog(ERROR, "invalid percentile value %f - needs to be in [0,1]", quantiles[i]);
}
