#BST order, bad JOIN
CREATE alpha 2 double string score letter
INSERT INTO alpha 5 ROWS
10 A
50 C
30 B
20 D
40 E
GENERATE FOR alpha bst INDEX ON letter
PRINT FROM alpha 2 score letter WHERE letter > B
GENERATE FOR alpha hash INDEX ON score
PRINT FROM alpha 2 score letter WHERE score < 50
DELETE FROM alpha WHERE score = 10
PRINT FROM alpha 2 score letter WHERE score < 50
CREATE beta 2 int string val word
INSERT INTO beta 3 ROWS
10 A
20 B
30 C
JOIN alpha AND beta WHERE letter = word AND PRINT 2 score 1 val 2
JOIN beta AND alpha WHERE score = val AND PRINT 2 val 1 score 2
REMOVE alpha
REMOVE beta
QUIT
