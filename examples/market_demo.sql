# Market-data demo: load a session of synthetic trades and analyse it.
CREATE trades 5 int string string double int ts symbol side price qty
LOAD INTO trades FROM examples/trades.csv
GENERATE FOR trades bst INDEX ON price
# An ordered-index range scan returns rows in price order.
EXPLAIN PRINT FROM trades 4 ts symbol price qty WHERE price < 129.6
PRINT FROM trades 4 ts symbol price qty WHERE price < 129.6
PRINT FROM trades 4 ts symbol price qty WHERE price >= 423
# Per-symbol and per-side summaries.
AGGREGATE FROM trades AVG price GROUP BY symbol
AGGREGATE FROM trades SUM qty GROUP BY side
AGGREGATE FROM trades COUNT ts GROUP BY symbol WHERE qty >= 500
EXPLAIN AGGREGATE FROM trades MAX price WHERE symbol = NVDA
GENERATE FOR trades hash INDEX ON symbol
EXPLAIN AGGREGATE FROM trades MAX price WHERE symbol = NVDA
AGGREGATE FROM trades MAX price WHERE symbol = NVDA
AGGREGATE FROM trades MIN price GROUP BY side WHERE symbol = MSFT
# Join reference data against the trade tape.
CREATE watchlist 2 string string symbol sector
INSERT INTO watchlist 2 ROWS
GOOG internet
TSLA autos
EXPLAIN JOIN watchlist AND trades WHERE symbol = symbol AND PRINT 4 sector 1 ts 2 price 2 qty 2
JOIN watchlist AND trades WHERE symbol = symbol AND PRINT 4 sector 1 ts 2 price 2 qty 2
# Drop odd lots, then re-aggregate.
DELETE FROM trades WHERE qty < 200
AGGREGATE FROM trades SUM qty GROUP BY symbol
QUIT
