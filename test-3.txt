# Self-JOIN tests with indices and filters
CREATE inventory 3 int string bool id item in_stock
INSERT INTO inventory 6 ROWS
101 hammer true
102 nails true
103 glue false
104 hammer true
105 nails false
106 glue true
PRINT FROM inventory 2 item id WHERE in_stock = true
JOIN inventory AND inventory WHERE item = item AND PRINT 3 item 1 id 1 in_stock 2
GENERATE FOR inventory hash INDEX ON item
JOIN inventory AND inventory WHERE item = item AND PRINT 2 id 1 id 2
GENERATE FOR inventory bst INDEX ON id
JOIN inventory AND inventory WHERE id = id AND PRINT 2 id 1 item 2
DELETE FROM inventory WHERE in_stock = false
JOIN inventory AND inventory WHERE item = item AND PRINT 2 item 1 id 2
REMOVE inventory
QUIT
