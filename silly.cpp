// Project Identifier: C0F4DFE8B340D81183C208F70F9D2D797908754D

#include <algorithm>
#include <getopt.h>
#include <iostream>
#include <string> 
#include "Field.h"
#include <unordered_map>
#include <map>
#include <vector>
#include <iostream>

using namespace std;


struct Options {
    bool quiet = false; 
};

enum class IndexType { 
    None, Hash, BST 
};


class Table{
    public:
    Table(const string& name, const vector<ColumnType>& types, const unordered_map<string, u_int32_t>& names)
        : table_name(name), col_types(types), col_names(names) {}

    string table_name;
    vector<ColumnType> col_types;
    vector<vector<Field>> data;
    unordered_map<string, u_int32_t> col_names;

    // Indexing support
    IndexType index_type = IndexType::None;
    size_t index_col = 0;
    unordered_map<Field, vector<size_t>> hash_index;
    map<Field, vector<size_t>> bst_index;
};

static std::unordered_map<std::string, Table> database;

void printHelp(const char *command) {
    cout << "Usage: " << command << " [options] < inputfile\n";
    cout << "Options:\n";
    cout << "  -h, --help              Show this help message and exit\n";  
    cout << "-q --quiet                Makes output run quieter\n";
}

void parseOptions(int argc, char **argv, Options &options) {
    int choice;
    int index = 0;
    static struct option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"quiet", no_argument, nullptr, 'q'},
        {nullptr, 0, nullptr, '\0'}
    };

    while ((choice = getopt_long(argc, argv, "hq", long_options, &index)) != -1) {
        switch (choice) {
        case 'h':
            printHelp(argv[0]);
            exit(0);
        case 'q':
            options.quiet = true;
            break;
        default:
            cerr << "Unknown command line option\n";
            exit(1); 
        }
    }
}

int main(int argc, char **argv) {
    ios_base::sync_with_stdio(false);
    cin >> std::boolalpha;  // add these two lines
    cout << std::boolalpha; // add these two lines

    Options options;
    parseOptions(argc, argv, options);

    string command;

    do {
        if (cin.fail()) {
            cerr << "Error: Reading from cin has failed\n";
            exit(1); // Stop on input failure
        }

        cout << "% ";

        cin >> command;


        if (command[0] == '#') { 
            string ignoreLine;
            getline(cin, ignoreLine); // Skip rest of the comment line
            continue; 
        }

        else if (command == "CREATE"){
            //implement create
            string table_name;
            size_t num_columns;
            cin >> table_name >> num_columns;
        
            // Check if table already exists
            if (database.find(table_name) != database.end()) {
                cout << "Error during CREATE: Cannot create already existing table " << table_name << endl;
                string ignoreLine;
                getline(cin, ignoreLine); // Clear the rest of the line
                continue;
            }
        
            vector<ColumnType> col_types;
            unordered_map<string, u_int32_t> col_names;
            vector<string> col_names_ordered;

            col_types.reserve(num_columns);
            col_names_ordered.reserve(num_columns);
        
            // Read column types
            for (size_t i = 0; i < num_columns; ++i) {
                string type;
                cin >> type;
        
                if (type == "string") {
                    col_types.push_back(ColumnType::String);
                } else if (type == "double") {
                    col_types.push_back(ColumnType::Double);
                } else if (type == "int") {
                    col_types.push_back(ColumnType::Int);
                } else if (type == "bool") {
                    col_types.push_back(ColumnType::Bool);
                } 
                
                //else {
                //    cout << "Error during CREATE: Invalid column type " << type << endl;
                //    string ignoreLine;
                //    getline(cin, ignoreLine); // Clear rest of the line
                //    continue;
                //}
            }
        
            // Read column names
            for (size_t i = 0; i < num_columns; ++i) {
                string col_name;
                cin >> col_name;
                col_names[col_name] = static_cast<int>(i); // Map column name to its index
                col_names_ordered.push_back(col_name);
            }
        
            // Create and insert the table
            database.emplace(table_name, Table(table_name, col_types, col_names));
        
            // Print confirmation
            cout << "New table " << table_name << " with column(s)";
            for (const auto& name : col_names_ordered) {
                cout << " " << name;
            }
            cout << " created" << endl;
        }


        else if(command == "REMOVE"){
                string table_name;
                cin >> table_name;
            
                // Check if table exists
                auto it = database.find(table_name);
                if (it == database.end()) {
                    cout << "Error during REMOVE: " << table_name << " does not name a table in the database" << endl;
                    string ignoreLine;
                    getline(cin, ignoreLine); // Clear rest of input line
                    continue;
                }
            
                // Erase the table from the database
                database.erase(it);
            
                // Print confirmation
                cout << "Table " << table_name << " removed" << endl;
        }

        else if(command == "INSERT"){   
                string junk;             
                string table_name;
                size_t num_rows;
            
                cin >> junk >> table_name >> num_rows;
                cin >> junk;  // Read table name and number of rows
            
                // Check if table exists
                auto it = database.find(table_name);
                if (it == database.end()) {
                    cout << "Error during INSERT: " << table_name << " does not name a table in the database" << endl;
                    string ignoreLine;
                    getline(cin, ignoreLine);  // Clear the rest of the input line
                    continue;
                }
            
                Table &table = it->second;
                size_t num_cols = table.col_types.size();
                size_t start_index = table.data.size();  // Index of first inserted row
            
                table.data.resize(start_index + num_rows);  // Reserve memory for efficiency
            
                // Insert rows
                for (size_t i = start_index; i < table.data.size(); ++i) {
                    //vector<Field> row;
                    table.data[i].reserve(num_cols);  // Prevents unnecessary reallocations
            
                    // Read and construct each field
                    for (size_t j = 0; j < num_cols; ++j) {
                        switch (table.col_types[j]) {
                            case ColumnType::String: {
                                string val;
                                cin >> val;
                                table.data[i].emplace_back(val);  // Avoid unnecessary string copies
                                if (table.index_type == IndexType::Hash && table.index_col == j ) {
                                    table.hash_index[Field(val)].push_back(i);
                                }    ///here 
                                else if (table.index_type == IndexType::BST && table.index_col == j ) {
                                    table.bst_index[Field(val)].push_back(i);   //Field(val)
                                }
                                break;
                            }
                            case ColumnType::Double: {
                                double val;
                                cin >> val;
                                table.data[i].emplace_back(val);
                                if (table.index_type == IndexType::Hash && table.index_col == j ) {
                                    table.hash_index[Field(val)].push_back(i);
                                }    ///here 
                                else if (table.index_type == IndexType::BST && table.index_col == j ) {
                                    table.bst_index[Field(val)].push_back(i);   //Field(val)
                                }
                                break;
                            }
                            case ColumnType::Int: {
                                int val;
                                cin >> val;
                                table.data[i].emplace_back(val);
                                if (table.index_type == IndexType::Hash && table.index_col == j ) {
                                    table.hash_index[Field(val)].push_back(i);
                                }    ///here 
                                else if (table.index_type == IndexType::BST && table.index_col == j ) {
                                    table.bst_index[Field(val)].push_back(i);   //Field(val)
                                }
                                break;
                            }
                            case ColumnType::Bool: {
                                bool val;
                                cin >> val;
                                table.data[i].emplace_back(val);
                                if (table.index_type == IndexType::Hash && table.index_col == j ) {
                                    table.hash_index[Field(val)].push_back(i);
                                }    ///here 
                                else if (table.index_type == IndexType::BST && table.index_col == j ) {
                                    table.bst_index[Field(val)].push_back(i);   //Field(val)
                                }
                                break;
                            }
                        }
                    }
                    
            
                    // Add row to table
                   // table.data.emplace_back(move(row));
                }
            
                // Print confirmation
                cout << "Added " << num_rows << " rows to " << table_name 
                     << " from position " << start_index << " to " 
                     << (start_index + num_rows - 1) << endl;
                    
        }

        else if (command == "PRINT"){
            string tableName;
            int length;
            string junkword;
            cin >> junkword >> tableName >> length;

            if (database.find(tableName) == database.end()) {
                cout << "Error during PRINT: " << tableName << " does not name a table in the database\n";
                getline(cin, junkword);
                continue;
            }

            string colName;
            vector<string> columns;
            vector<int> indices;

            auto current = database.find(tableName);
            for (int i = 0; i < length; ++i) {
                cin >> colName;
                if (current->second.col_names.count(colName) == 0) {  // used to befind(current->second.col_names.begin(), current->second.col_names.end(),colName) == current->second.col_names.end()
                    cout << "Error during PRINT: " << colName << " does not name a column in " << tableName << "\n";
                    getline(cin, junkword);
                    continue;
                }
                columns.push_back(colName);
                indices.push_back(current->second.col_names[colName]);
            }

            string mode;

            cin >> mode;

            if(mode == "ALL"){
                const Table& table = database.at(tableName);

                // Header row (column names)
                if (!options.quiet) {
                    for (const auto& name : columns) {
                        cout << name << " ";
                    }
                    cout << "\n";
            
                    // Print each row in insertion order
                    for (const auto& row : table.data) {
                        for (int idx : indices) {
                            cout << row[idx] << " ";
                        }
                        cout << "\n";
                    }
                }
                // Always print summary
                cout << "Printed " << table.data.size() << " matching rows from " << tableName << "\n";
            }

            else {
                //printwhere();

                string where_col, op;
                cin >> where_col >> op;
            
                // Validate WHERE column
                if (current->second.col_names.count(where_col) == 0) {
                    cout << "Error during PRINT: " << where_col << " does not name a column in " << tableName << "\n";
                    getline(cin, junkword);
                    continue;
                }
            
                Table& table = current->second;
                size_t where_idx = table.col_names[where_col];
                ColumnType type = table.col_types[where_idx];
            
                // Build filter Field
                Field filter = [&]() -> Field {
                    switch (type) {
                        case ColumnType::String: { string v; cin >> v; return Field(v); }
                        case ColumnType::Int: { int v; cin >> v; return Field(v); }
                        case ColumnType::Double: { double v; cin >> v; return Field(v); }
                        case ColumnType::Bool: { bool v; cin >> v; return Field(v); }
                    }
                    return Field(0); // won't hit
                }();
            
                // Print header if not quiet
                if (!options.quiet) {
                    for (const string& col : columns) {
                        cout << col << " ";
                    }
                    cout << "\n";
                }
            
                size_t match_count = 0;
                bool used_index = false;
            
                // ✅ Hash index and op is "="
                if (table.index_col == where_idx && table.index_type == IndexType::Hash && op == "=") {
                    used_index = true;
                    auto it = table.hash_index.find(filter);
                    if (it != table.hash_index.end()) {
                        for (size_t idx : it->second) {
                            const auto& row = table.data[idx];
                            if (row[where_idx] == filter) {
                                if (!options.quiet) {
                                    for (int col : indices) cout << row[col] << " ";
                                    cout << "\n";
                                }
                                ++match_count;
                            }
                        }
                    }
                }
            
                // ✅ BST index: all ops supported
                else if (table.index_col == where_idx && table.index_type == IndexType::BST) {   
                    used_index = true;
                    if (op == "=") {
                        auto it = table.bst_index.find(filter);
                        if (it != table.bst_index.end()) {
                            for (size_t idx : it->second) {
                                const auto& row = table.data[idx];
                                if (row[where_idx] == filter) {
                                    if (!options.quiet) {
                                        for (int col : indices) cout << row[col] << " ";
                                        cout << "\n";
                                    }
                                    ++match_count;
                                }
                            }
                        }
                    } else if (op == "<") {
                        for (auto it = table.bst_index.begin(); it != table.bst_index.lower_bound(filter); ++it) { //&& it->first < filter
                            for (size_t idx : it->second) {
                                const auto& row = table.data[idx];
                                if (row[where_idx] < filter) {
                                    if (!options.quiet) {
                                        for (int col : indices) cout << row[col] << " ";    //Maybe need to reverse loop
                                        cout << "\n";
                                    }
                                    ++match_count;
                                }
                            }
                        }
                    } else if (op == ">") {
                        for (auto it = table.bst_index.upper_bound(filter); it != table.bst_index.end(); ++it) {
                            for (size_t idx : it->second) {
                                const auto& row = table.data[idx];
                                if (row[where_idx] > filter ) {
                                    if (!options.quiet) {
                                        for (int col : indices) cout << row[col] << " ";
                                        cout << "\n";
                                    }
                                    ++match_count;
                                }
                            }
                        }
                    } else {
                        used_index = false; // unsupported operator
                    }
                }
            
                // 🔁 Fallback: linear scan
                if (!used_index) {
                    for (const auto& row : table.data) {
                        const Field& cell = row[where_idx];
                        if ((op == "=" && cell == filter) ||
                            (op == "<" && cell < filter) ||
                            (op == ">" && filter < cell)) {
                            if (!options.quiet) {
                                for (int col : indices) cout << row[col] << " ";
                                cout << "\n";
                            }
                            ++match_count;
                        }
                    }
                }
                // Summary
                cout << "Printed " << match_count << " matching rows from " << tableName << "\n";
            }  
        }


        else if (command == "DELETE") {
            string junk, table_name, col_name, op;
            cin >> junk >> table_name >> junk >> col_name >> op;
        
            // Check table
            auto it = database.find(table_name);
            if (it == database.end()) {
                cout << "Error during DELETE: " << table_name << " does not name a table in the database" << endl;
                string ignoreLine;
                getline(cin, ignoreLine);
                continue;
            }
        
            Table& table = it->second;
        
            // Check column
            if (table.col_names.find(col_name) == table.col_names.end()) {
                cout << "Error during DELETE: " << col_name << " does not name a column in " << table_name << endl;
                string ignoreLine;
                getline(cin, ignoreLine);
                continue;
            }
        
            size_t col_idx = table.col_names[col_name];
            ColumnType type = table.col_types[col_idx];
        
            // Read target value and build Field
            Field target = [&]() -> Field {
                switch (type) {
                    case ColumnType::String: { string val; cin >> val; return Field(val); }
                    case ColumnType::Int:    { int val; cin >> val; return Field(val); }
                    case ColumnType::Double: { double val; cin >> val; return Field(val); }
                    case ColumnType::Bool:   { bool val; cin >> val; return Field(val); }
                }
                return Field(0); // never reached
            }();
        
            vector<bool> to_delete(table.data.size(), false);
            bool used_index = false;
        
            if (table.index_col == col_idx && table.index_type != IndexType::None) {
                used_index = true;
        
                if (table.index_type == IndexType::Hash && op == "=") {
                    auto it = table.hash_index.find(target);
                    if (it != table.hash_index.end()) {
                        for (size_t idx : it->second) {
                            if (idx < table.data.size()) {
                                const Field& cell = table.data[idx][col_idx];
                                if (cell == target) to_delete[idx] = true;
                            }
                        }
                    }
                } else if (table.index_type == IndexType::BST) {
                    auto& bst = table.bst_index;
                    if (op == "=") {
                        auto it = bst.find(target);
                        if (it != bst.end()) {
                            for (size_t idx : it->second) {
                                if (idx < table.data.size()) {
                                    const Field& cell = table.data[idx][col_idx];
                                    if (cell == target) to_delete[idx] = true;
                                }
                            }
                        }
                    } else if (op == "<") {
                        for (auto it = bst.begin(); it != bst.end() && it->first < target; ++it) {
                            for (size_t idx : it->second) {
                                if (idx < table.data.size()) {
                                    const Field& cell = table.data[idx][col_idx];
                                    if (cell < target) to_delete[idx] = true;
                                }
                            }
                        }
                    } else if (op == ">") {
                        for (auto it = bst.upper_bound(target); it != bst.end(); ++it) {
                            for (size_t idx : it->second) {
                                if (idx < table.data.size()) {
                                    const Field& cell = table.data[idx][col_idx];
                                    if (target < cell) to_delete[idx] = true;
                                }
                            }
                        }
                    } else {
                        used_index = false; // fallback to full scan if op not supported
                    }
                } else {
                    used_index = false; // fallback if operator is unsupported
                }
            }
        
            if (!used_index) {
                for (size_t i = 0; i < table.data.size(); ++i) {
                    const Field& cell = table.data[i][col_idx];
                    if ((op == "=" && cell == target) ||
                        (op == "<" && cell < target) ||
                        (op == ">" && target < cell)) {
                        to_delete[i] = true;
                    }
                }
            }
        
            size_t deleted = 0;
            vector<vector<Field>> new_data;
            for (size_t i = 0; i < table.data.size(); ++i) {
                if (!to_delete[i]) {
                    new_data.push_back(std::move(table.data[i]));
                } else {
                    ++deleted;
                }
            }
        
            table.data = std::move(new_data);
        
            cout << "Deleted " << deleted << " rows from " << table_name << endl;


            if (deleted > 0 && table.index_type != IndexType::None) {
                // We already know table.index_col is the column with the index
                //size_t idx = table.col_names[table.index_col];
        
                // Clear old index
                table.hash_index.clear();
                table.bst_index.clear();
        
                // Rebuild it just like "GENERATE" does
                if (table.index_type == IndexType::Hash) {
                    for (size_t i = 0; i < table.data.size(); ++i) {
                        table.hash_index[ table.data[i][table.index_col] ].push_back(i);
                    }
                }
                else if (table.index_type == IndexType::BST) {
                    for (size_t i = 0; i < table.data.size(); ++i) {
                        table.bst_index[ table.data[i][table.index_col] ].push_back(i);
                    }
                }
                // index_col and index_type remain sets
            }
        }
        
        else if (command == "GENERATE") {
            string junk, table_name, index_type_str, col_name;
        
            // Parse: GENERATE FOR <table_name> <index_type> INDEX ON <col_name>
            cin >> junk >> table_name >> index_type_str >> junk >> junk >> col_name;
        
            auto it = database.find(table_name);
            if (it == database.end()) {
                cout << "Error during GENERATE: " << table_name
                     << " does not name a table in the database" << endl;
                getline(cin, junk);
                continue;
            }
        
            Table& table = it->second;
        
            auto col_it = table.col_names.find(col_name);
            if (col_it == table.col_names.end()) {
                cout << "Error during GENERATE: " << col_name
                     << " does not name a column in " << table_name << endl;
                getline(cin, junk);
                continue;
            }
        
            size_t col_index = col_it->second;
        
            // Clear any existing index
            table.hash_index.clear();
            table.bst_index.clear();
            table.index_col = col_index;
        
            size_t distinct_keys = 0;
        
            if (index_type_str == "hash") {
                table.index_type = IndexType::Hash;
                for (size_t i = 0; i < table.data.size(); ++i) {
                    table.hash_index[table.data[i][col_index]].push_back(i);
                }
                distinct_keys = table.hash_index.size();
            } else if (index_type_str == "bst") {
                table.index_type = IndexType::BST;
                for (size_t i = 0; i < table.data.size(); ++i) {
                    table.bst_index[table.data[i][col_index]].push_back(i);
                }
                distinct_keys = table.bst_index.size();
            } 
        
            cout << "Generated " << index_type_str << " index for table " << table_name
                 << " on column " << col_name << ", with " << distinct_keys << " distinct keys" << endl;
        }

        else if (command == "JOIN") {
            string table1_name, and_kw, table2_name, where_kw;
            string col1, eq_sign, col2;
            string and_kw2, print_kw;
            int num_columns;
        
            cin >> table1_name >> and_kw >> table2_name >> where_kw;
            cin >> col1 >> eq_sign >> col2 >> and_kw2 >> print_kw >> num_columns;
        
            if (database.find(table1_name) == database.end()) {
                cout << "Error during JOIN: " << table1_name << " does not name a table in the database" << endl;
                string ignore; getline(cin, ignore); continue;
            }
            if (database.find(table2_name) == database.end()) {
                cout << "Error during JOIN: " << table2_name << " does not name a table in the database" << endl;
                string ignore; getline(cin, ignore); continue;
            }
        
            Table& table1 = database.at(table1_name);
            Table& table2 = database.at(table2_name);
        
            if (table1.col_names.find(col1) == table1.col_names.end()) {
                cout << "Error during JOIN: " << col1 << " does not name a column in " << table1_name << endl;
                string ignore; getline(cin, ignore); continue;
            }
            if (table2.col_names.find(col2) == table2.col_names.end()) {
                cout << "Error during JOIN: " << col2 << " does not name a column in " << table2_name << endl;
                string ignore; getline(cin, ignore); continue;
            }
        
            size_t col1_idx = table1.col_names[col1];
            size_t col2_idx = table2.col_names[col2];
        
            vector<pair<size_t, const Table*>> print_indices;
            vector<string> print_headers;
        
            for (int i = 0; i < num_columns; ++i) {
                string col_name; int table_num;
                cin >> col_name >> table_num;
        
                const Table* t = (table_num == 1) ? &table1 : &table2;
                if (t->col_names.find(col_name) == t->col_names.end()) {
                    cout << "Error during JOIN: " << col_name << " does not name a column in "
                         << ((table_num == 1) ? table1_name : table2_name) << endl;
                    string ignore; getline(cin, ignore); continue;
                }
        
                print_indices.emplace_back(t->col_names.at(col_name), t);
                print_headers.push_back(col_name);
            }
        
            if (!options.quiet) {
                for (const string& name : print_headers) cout << name << " ";
                cout << "\n";
            }
        
            size_t match_count = 0;
        
            bool use_existing_index = (table2.index_type != IndexType::None && table2.index_col == col2_idx);
            unordered_map<Field, vector<const vector<Field>*>> temp_hash_index;
        
            if (!use_existing_index) {
                for (const auto& row : table2.data) {
                    temp_hash_index[row[col2_idx]].push_back(&row);
                }
            }
        
            for (const auto& row1 : table1.data) {
                const Field& key = row1[col1_idx];
                vector<const vector<Field>*> matching_rows;
        
                if (use_existing_index) {
                    if (table2.index_type == IndexType::Hash) {
                        auto it = table2.hash_index.find(key);
                        if (it != table2.hash_index.end()) {
                            for (size_t idx : it->second)
                                matching_rows.push_back(&table2.data[idx]);
                        }
                    } else { // BST
                        auto it = table2.bst_index.find(key);
                        if (it != table2.bst_index.end()) {
                            for (size_t idx : it->second)
                                matching_rows.push_back(&table2.data[idx]);
                        }
                    }
                } else {
                    auto it = temp_hash_index.find(key);
                    if (it != temp_hash_index.end()) {
                        matching_rows = it->second;
                    }
                }
        
                for (const auto* row2 : matching_rows) {
                    if (!options.quiet) {
                        for (const auto& [idx, t] : print_indices) {
                            const vector<Field>& row = (t == &table1) ? row1 : *row2;
                            cout << row[idx] << " ";
                        }
                        cout << "\n";
                    }
                    ++match_count;
                }
            }
        
            cout << "Printed " << match_count << " rows from joining "
                 << table1_name << " to " << table2_name << "\n";
        }    

        else if (command != "QUIT") {
			cout << "Error: unrecognized command\n";
            //cout << "Error: unknown command line option\n";
			string ignoreLine;
			getline(cin, ignoreLine);
		}

    } while (command != "QUIT");
        cout << "Thanks for being silly!\n";   

    return 0;
}