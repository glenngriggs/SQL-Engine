# Extended comparison operators <=, >=, != through every access path
CREATE quotes 4 string double int bool symbol price size firm
INSERT INTO quotes 8 ROWS
AAPL 190.5 100 true
MSFT 420.25 300 false
AAPL 189.75 200 true
NVDA 131.5 500 false
MSFT 419.5 100 true
NVDA 130.25 400 false
AAPL 190.5 300 false
GOOG 171 100 true
PRINT FROM quotes 3 symbol price size WHERE price <= 190.5
PRINT FROM quotes 3 symbol price size WHERE price >= 190.5
PRINT FROM quotes 2 symbol size WHERE symbol != AAPL
PRINT FROM quotes 2 symbol firm WHERE firm != true
GENERATE FOR quotes hash INDEX ON symbol
PRINT FROM quotes 2 symbol size WHERE symbol != AAPL
PRINT FROM quotes 2 symbol size WHERE symbol >= MSFT
PRINT FROM quotes 2 symbol size WHERE symbol = AAPL
GENERATE FOR quotes bst INDEX ON price
PRINT FROM quotes 3 symbol price size WHERE price <= 190.5
PRINT FROM quotes 3 symbol price size WHERE price >= 190.5
PRINT FROM quotes 3 symbol price size WHERE price != 190.5
PRINT FROM quotes 3 symbol price size WHERE price = 190.5
PRINT FROM quotes 3 symbol price size WHERE price < 131.5
PRINT FROM quotes 3 symbol price size WHERE price > 400
DELETE FROM quotes WHERE size <= 100
PRINT FROM quotes 3 symbol price size ALL
DELETE FROM quotes WHERE price >= 420
PRINT FROM quotes 3 symbol price size WHERE price > 0
DELETE FROM quotes WHERE symbol != NVDA
PRINT FROM quotes 3 symbol price size ALL
PRINT FROM quotes 1 symbol WHERE price ~ 3
DELETE FROM quotes WHERE price => 3
PRINT FROM quotes 1 symbol ALL
QUIT
