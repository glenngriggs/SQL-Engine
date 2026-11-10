#DELETE behavior
CREATE db 3 string int bool key val flag
INSERT INTO db 3 ROWS
a 1 false
b 2 false
c 3 false
PRINT FROM db 3 flag val key ALL
DELETE FROM db WHERE key = x
PRINT FROM db 3 flag val key ALL
INSERT INTO db 2 ROWS
x 4 true
x 5 false
PRINT FROM db 3 flag val key ALL
DELETE FROM db WHERE key = x
PRINT FROM db 3 flag val key ALL
DELETE FROM db WHERE flag = false
PRINT FROM db 3 flag val key WHERE flag = false
PRINT FROM db 3 flag val key ALL
REMOVE db
QUIT
