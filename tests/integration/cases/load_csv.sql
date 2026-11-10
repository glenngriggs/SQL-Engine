# LOAD: CSV import with header binding, type checking and index maintenance
CREATE orders 4 int string double bool id trader notional urgent
GENERATE FOR orders hash INDEX ON trader
LOAD INTO orders FROM tests/integration/data/orders.csv
PRINT FROM orders 4 id trader notional urgent ALL
PRINT FROM orders 2 id notional WHERE trader = lee
LOAD INTO orders FROM tests/integration/data/orders_more.csv
PRINT FROM orders 2 id notional WHERE trader = lee
LOAD INTO orders FROM tests/integration/data/does_not_exist.csv
LOAD INTO orders FROM tests/integration/data/empty.csv
LOAD INTO orders FROM tests/integration/data/orders_unknown_column.csv
LOAD INTO orders FROM tests/integration/data/orders_duplicate_column.csv
LOAD INTO orders FROM tests/integration/data/orders_missing_column.csv
LOAD INTO orders FROM tests/integration/data/orders_bad_value.csv
LOAD INTO orders FROM tests/integration/data/orders_bad_field_count.csv
LOAD INTO orders FROM tests/integration/data/orders_bad_quote.csv
LOAD INTO orders FROM tests/integration/data/orders_spaced_string.csv
LOAD INTO orders FROM tests/integration/data/orders_empty_field.csv
LOAD INTO orders FROM tests/integration/data
LOAD INTO orders FROM tests/integration/data/orders_header_only.csv
LOAD INTO nope FROM tests/integration/data/orders.csv
LOAD ONTO orders FROM tests/integration/data/orders.csv
PRINT FROM orders 2 id trader ALL
GENERATE FOR orders hash INDEX ON trader
QUIT
