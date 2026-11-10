# EXPLAIN: access-path and join-strategy selection, without executing
CREATE trades 4 int string double int id symbol price qty
INSERT INTO trades 4 ROWS
1 AAPL 190.5 100
2 MSFT 420.25 300
3 AAPL 189.75 200
4 NVDA 131.5 500
CREATE venues 2 string string symbol venue
INSERT INTO venues 2 ROWS
AAPL XNAS
MSFT XNAS
EXPLAIN PRINT FROM trades 2 id price ALL
EXPLAIN PRINT FROM trades 2 id price WHERE qty > 100
EXPLAIN JOIN trades AND venues WHERE symbol = symbol AND PRINT 2 id 1 venue 2
GENERATE FOR trades hash INDEX ON symbol
EXPLAIN PRINT FROM trades 2 id price WHERE symbol = AAPL
EXPLAIN PRINT FROM trades 2 id price WHERE symbol < AAPL
EXPLAIN DELETE FROM trades WHERE symbol = AAPL
EXPLAIN AGGREGATE FROM trades SUM qty GROUP BY symbol WHERE symbol = MSFT
EXPLAIN AGGREGATE FROM trades COUNT id
GENERATE FOR trades bst INDEX ON price
EXPLAIN PRINT FROM trades 1 id WHERE price < 190
EXPLAIN PRINT FROM trades 1 id WHERE price >= 190
EXPLAIN PRINT FROM trades 1 id WHERE price = 190.5
EXPLAIN PRINT FROM trades 1 id WHERE price != 190.5
EXPLAIN PRINT FROM trades 1 id WHERE symbol = AAPL
EXPLAIN DELETE FROM trades WHERE price > 100
PRINT FROM trades 2 id price ALL
GENERATE FOR venues hash INDEX ON symbol
EXPLAIN JOIN trades AND venues WHERE symbol = symbol AND PRINT 2 id 1 venue 2
GENERATE FOR venues bst INDEX ON symbol
EXPLAIN JOIN trades AND venues WHERE symbol = symbol AND PRINT 2 id 1 venue 2
EXPLAIN JOIN venues AND trades WHERE symbol = symbol AND PRINT 2 venue 1 id 2
JOIN trades AND venues WHERE symbol = symbol AND PRINT 2 id 1 venue 2
EXPLAIN QUIT
EXPLAIN PRINT FROM nope 1 id ALL
EXPLAIN PRINT FROM trades 1 nope ALL
QUIT
