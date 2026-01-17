# ⭐ NyotaDB  
**A Simple Relational Database Management System (RDBMS) in C**

> *“Not just another database, but a journey through database implementation.”*

NyotaDB (*Swahili for “Star Database”*) is a lightweight, educational RDBMS built **from scratch in C**.  
It demonstrates how real databases work internally — from SQL parsing and query execution to page-based storage, indexing, and caching — all in a compact, readable codebase.

---

## 📖 Overview

NyotaDB is designed as a **technical challenge and learning project**, showcasing core database system concepts:

- Storage engines and page management
- SQL parsing and execution
- B-Tree indexing
- LRU buffer cache
- Command-line and web interfaces

It is **not intended for production use**, but rather as a clear, inspectable reference for understanding database internals.

---

## ✨ Features

### 🔹 Core Database Engine
- SQL Parser: CREATE, SELECT, INSERT, UPDATE, DELETE, SHOW TABLES
- Page-based storage with LRU caching
- B-Tree indexing for primary keys
- Full CRUD query execution
- Data types: INT, FLOAT, STRING, BOOL

### 🔹 Interfaces
- Interactive CLI (REPL)
- HTTP/JSON Web Server
- Single binary, multi-mode operation

### 🔹  SQL Support
```sql
CREATE TABLE users (id INT PRIMARY KEY, name STRING(50), age INT);
SHOW TABLES;
DROP TABLE users;

-- Data Operations  
INSERT INTO users VALUES (1, 'Alice', 25);
SELECT * FROM users WHERE age > 20;
UPDATE users SET age = 26 WHERE id = 1;
DELETE FROM users WHERE id = 2;

```

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────┐
│                 Interfaces                  │
│                                             │
│   ┌───────────────┐   ┌─────────────────┐   │
│   │     REPL      │   │   Web Server    │   │
│   │   (CLI)       │   │   (HTTP/JSON)   │   │
│   └───────────────┘   └─────────────────┘   │
└─────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────┐
│              Query Processing               │
│                                             │
│   ┌───────────────┐   ┌─────────────────┐   │
│   │   SQL Parser  │ → │   Executor      │   │
│   │ (Tokenizer +  │   │   (CRUD Ops)    │   │
│   │  AST Builder) │   │                 │   │
│   └───────────────┘   └─────────────────┘   │
└─────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────┐
│              Storage Engine                 │
│                                             │
│   ┌───────────────┐   ┌─────────────────┐   │
│   │    B-Tree     │   │  Page Manager   │   │
│   │    Index      │   │  (LRU Cache)    │   │
│   └───────────────┘   └─────────────────┘   │
└─────────────────────────────────────────────┘
                    │
                    ▼
               Disk Storage
                nyotadb.db
```

---

## 📦 Project Structure

```
nyotaDB/
├── rdbms/                    # Core database engine
│   ├── main.c               # Entry point
│   ├── storage.h/.c         # Page manager + LRU cache
│   ├── btree.h/.c           # B-Tree index
│   ├── parser.h/.c          # SQL parser
│   ├── executor.h/.c        # Query executor
│   ├── repl.h/.c            # CLI REPL
│   └── webserver.h/.c       # HTTP/JSON server
├── webapp/                   # Frontend assets (optional)
├── Makefile
├── run.sh
└── README.md
```

---

## 🚀 Quick Start

```bash
# clone the repository
git clone https://github.com/yourusername/nyotadb.git
cd nyotadb

# Build the project
make

# Or use the build scrip
./run.sh
```

---

## ▶️ Running NyotaDB

```bash
./nyotadb
```

Example:

```
nyotadb> CREATE TABLE users (id INT PRIMARY KEY, name STRING(50), age INT);
nyotadb> INSERT INTO users VALUES (1, 'Alice', 25);
nyotadb> SELECT * FROM users;
nyotadb> .tables
nyotadb> .schema users
nyotadb> EXIT;
```

---

### Web Server Mode

```bash
./nyotadb --web
```

Open: http://localhost:8080

---

## 📚 API Documentation

REPL Commands

| Command | Description |
|---------|-------------|
| CREATE TABLE ... | Create new table |
| INSERT INTO ... VALUES ... | Insert data |
| SELECT ... FROM ... | Query data |
| UPDATE ... SET ... | Update data |
| DELETE FROM ... | Delete data |
| SHOW TABLES | List all tables |
| HELP | Show help |
| QUIT or EXIT | Exit REPL |
| .tables | List tables |
| .schema \<table> | Show table schema |
| .clear | Clear screen |
| .stats | Show database stats |

---


**POST** `/api/query`

```json
{
  "query": "SELECT * FROM users WHERE age > 25"
}
```

Response:

```json
{
  "columns": ["id", "name", "age"],
  "rows": [[2, "Bob", 30]],
  "rowCount": 1
}
```

---

## 🔧 Technical Details

### Storage
- Header + contiguous 4KB pages
- Schema pages for metadata
- Deleted flag + row ID + column data

### B-Tree
- Order 4 (2-3-4 tree)
- Persistent nodes
- Search and insert operations

### LRU Cache
- 100-page cache
- True LRU eviction
- Write-back persistence

### Parser Features
- Recursive descent parser
- Tokenizer with lookahead
- Error reporting with line/position
- Support for quoted strings, integers, floats

---

## 📊 Performance Characteristics

| Operation  |  Complexity  | Notes |
|------------|--------------|---------|
| INSERT	  |O(log n) |	With B-Tree index
| SELECT	  |O(log n) |	Indexed search
| SELECT	  |O(n) |	Full table scan
| UPDATE	  |O(log n) |	With WHERE clause
| DELETE	  |O(log n) |	With WHERE clause
| Cache Hit	  |O(1) |	Hash table lookup
| Cache Miss   |O(n) |	Disk I/O + LRU update

---

## 🙏 Acknowledgments

- Inspired by: SQLite
- Educational Resources:
  - "Database System Concepts" by Silberschatz, Korth, Sudarshan
  - "The Design and Implementation of the SQLite Database  Engine"
  - CMU 15-445/645 Database Systems course
- Tools: GCC, Make, Valgrind, GDB

---

## 🐛 Known Limitations

- **Concurrency**: No transaction support or locking
- **Durability**: Simple write-back, no WAL
- **Query Optimization**: No query planner, simple scans
- **Data Types**: Limited type system
- **Constraints**: Basic primary key only
- **Security**: No authentication/authorization

---

**NyotaDB** - Swahili for "Star Database" - Shining light on database internals ✨

"Not just another database, but a journey through database implementation"
