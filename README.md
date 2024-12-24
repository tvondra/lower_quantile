# Lower Quantile aggregate

[![make installcheck](https://github.com/tvondra/lower_quantile/actions/workflows/ci.yml/badge.svg)](https://github.com/tvondra/lower_quantile/actions/workflows/ci.yml)
[![PGXN version](https://badge.fury.io/pg/lower_quantile.svg)](https://badge.fury.io/pg/lower_quantile)

This implements the "lower quantile" aggregate, defined as

   Given a multiset S of size n over R, the q-quantile item xq (in S) is
   the item x whose rank R(x) in the sorted multiset S is
   
   floor(1 + q(n −1)) for 0 <= q <= 1
   
   where the rank R(x) is the number of elements in S smaller than or
   equal to x.

This is slightly different from the percentile_disc definition, which
returns the first value exceeding the threshold. For most cases this
different is not important, but some papers (e.g. the ddsketch paper)
formulate guarantees based on this definition.


## Installation

Installing this is very simple, especially if you're using pgxn client.
All you need to do is this:

    $ pgxn install lower_quantile
    $ pgxn load -d mydb lower_quantile

and you're done. You may also install the extension manually:

    $ make install
    $ psql dbname -c "CREATE EXTENSION lower_quantile"

And if you're on an older version (pre-9.1), you have to run the SQL
script manually

    $ psql dbname < `pg_config --sharedir`/contrib/lower_quantile--1.0.0.sql

That's all.


## License

This software is distributed under the terms of BSD 2-clause license.
See LICENSE or http://www.opensource.org/licenses/bsd-license.php for
more details.
