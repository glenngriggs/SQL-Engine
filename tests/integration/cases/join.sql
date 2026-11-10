#This is the example JOIN from the project video.
CREATE Cars_Marketplace 4 string string int double Vendor Model Miles Price
#Note that you can put extra spaces in the data to make it easier to read!
INSERT INTO Cars_Marketplace 7 ROWS
Chevrolet Corvette 67226 25965.00
Chevrolet Malibu   97874  7273.00
Chevrolet Malibu   15600 27441.00
Chevrolet Corvette 27982 50209.99
Ford      Mustang   8437 63901.00
Chevrolet Malibu   52477 46700.95
Ford      Fusion   43801 73429.00
CREATE profcars 3 string string string name vendor color
INSERT INTO profcars 4 ROWS
Kamil     Ford       Gray
Paoletti  Chevrolet  Blue
Garcia    Ford       Red
DeOrio    Subaru     White
JOIN Cars_Marketplace AND profcars WHERE Vendor = vendor AND PRINT 3 Model 1 name 2 color 2
QUIT
