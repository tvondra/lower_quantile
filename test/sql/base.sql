\set ECHO none

-- disable the notices for the create script (shell types etc.)
SET client_min_messages = 'WARNING';
\i sql/lower_quantile--1.0.0.sql
\i sql/lower_quantile--1.0.0--1.0.1.sql
\i sql/lower_quantile--1.0.1--1.0.2.sql
\i sql/lower_quantile--1.0.2--1.0.3.sql
SET client_min_messages = 'NOTICE';

\set ECHO all

SELECT lower_quantile(x, 0.01::double precision) FROM generate_series(1,1000) s(x);
SELECT lower_quantile(x, 0.01::numeric) FROM generate_series(1,1000) s(x);

SELECT lower_quantile(x, ARRAY[0.01, 0.05, 0.1, 0.9, 0.95, 0.99]::double precision[]) FROM generate_series(1,1000) s(x);
SELECT lower_quantile(x, ARRAY[0.01, 0.05, 0.1, 0.9, 0.95, 0.99]::numeric[]) FROM generate_series(1,1000) s(x);
