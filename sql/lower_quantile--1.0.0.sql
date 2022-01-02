/* quantile for the double precision */
CREATE OR REPLACE FUNCTION lower_quantile_append_double(p_pointer internal, p_element double precision, p_quantile double precision)
    RETURNS internal
    AS 'lower_quantile', 'quantile_append_double'
    LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION lower_quantile_append_double_array(p_pointer internal, p_element double precision, p_quantiles double precision[])
    RETURNS internal
    AS 'lower_quantile', 'quantile_append_double_array'
    LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION lower_quantile_append_double_numeric(p_pointer internal, p_element double precision, p_quantile numeric)
    RETURNS internal
    AS 'lower_quantile', 'quantile_append_double_numeric'
    LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION lower_quantile_append_double_array_numeric(p_pointer internal, p_element double precision, p_quantiles numeric[])
    RETURNS internal
    AS 'lower_quantile', 'quantile_append_double_array_numeric'
    LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION lower_quantile_double(p_pointer internal)
    RETURNS double precision
    AS 'lower_quantile', 'quantile_double'
    LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION lower_quantile_double_array(p_pointer internal)
    RETURNS double precision[]
    AS 'lower_quantile', 'quantile_double_array'
    LANGUAGE C IMMUTABLE;

CREATE AGGREGATE lower_quantile(double precision, double precision) (
    SFUNC = lower_quantile_append_double,
    STYPE = internal,
    FINALFUNC = lower_quantile_double
);

CREATE AGGREGATE lower_quantile(double precision, double precision[]) (
    SFUNC = lower_quantile_append_double_array,
    STYPE = internal,
    FINALFUNC = lower_quantile_double_array
);

CREATE AGGREGATE lower_quantile(double precision, numeric) (
    SFUNC = lower_quantile_append_double_numeric,
    STYPE = internal,
    FINALFUNC = lower_quantile_double
);

CREATE AGGREGATE lower_quantile(double precision, numeric[]) (
    SFUNC = lower_quantile_append_double_array_numeric,
    STYPE = internal,
    FINALFUNC = lower_quantile_double_array
);
