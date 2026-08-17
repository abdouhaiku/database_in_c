# miniDB

A small relational database engine written from scratch in C.

**Status:** personal educational project. Phases 1–7 complete, Phase 8 (CRUD) mostly complete.

---

## About

miniDB is a hand-built relational database: a SQL lexer and parser, a query planner, a pull-based execution engine, and a B+ tree storage engine sitting on top of a page-based file, all written from scratch in C11.

This is an educational project, not production software. The goal was to actually learn how a database works internally: B+ trees (insertion, splitting, deletion), how SQL text becomes an executable plan via an Abstract Syntax Tree, and the kind of low-level C, manual memory management, pointer arithmetic, byte-level serialization, that these data structures require.

Before writing any code, the project started from a written PRD (Product Requirements Document): goals, non-goals, and a 10-phase roadmap with explicit scope and acceptance criteria per phase. The roadmap section below follows that same phase breakdown.

---

## What it can do today

- `CREATE TABLE` with typed, multi-column schemas (`INTEGER` / `TEXT` / `BOOLEAN`, one `PRIMARY KEY` column)
- `INSERT`, `SELECT`, `UPDATE`, `DELETE`
- `WHERE` filtering and column projection
- A query planner that automatically picks a primary-key lookup over a full table scan when possible
- Data persists to disk and survives a restart
- Multiple, independently-schemaed tables in one database file
- An interactive REPL with debug/introspection commands: `.tables`, `.schema <table>`, `.tokens <sql>`, `.ast <sql>`, `.plan <sql>`, `.btree`

---

## Architecture

```
SQL text
  -> Lexer               (src/lexer.c)
  -> Parser               (src/parser.c)        -> Abstract Syntax Tree
  -> Semantic Analysis     (validate_columns)
  -> Query Planner         (build_plan)           -> PlanNode (scan / PK lookup / filter / projection)
  -> Cursor Execution      (build_cursor, cursor_next)  -> pulls rows one at a time
  -> B+ Tree Storage       (src/btree.c)
  -> Pager / Disk I/O      (src/pager.c)
```

| File | Responsibility |
|---|---|
| `src/repl.c` | Input loop, prompt |
| `src/lexer.c` | Tokenizes raw SQL text |
| `src/parser.c` | Recursive-descent parser, builds the AST |
| `src/ast.c` | AST, plan, and cursor types, and their destructors |
| `src/command.c` | Meta-commands (`.tables`, `.plan`, ...) and statement dispatch |
| `src/table.c` | Row (de)serialization, INSERT/UPDATE/DELETE execution, cursor operators |
| `src/btree.c` | Leaf/internal node layout, splitting, catalog |
| `src/pager.c` | Page cache and disk I/O |

---

## Getting started

Requires a C11 compiler and CMake.

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --target database_in_c
./cmake-build-debug/database_in_c
```

---

## Example session

```
miniDB> CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);
miniDB> INSERT INTO users VALUES (1, 'alice', 30);
miniDB> INSERT INTO users VALUES (2, 'bob', 25);
miniDB> SELECT * FROM users;
1 alice 30
2 bob 25
miniDB> UPDATE users SET age = 31 WHERE id = 1;
miniDB> SELECT name, age FROM users WHERE id = 1;
alice 31
miniDB> DELETE FROM users WHERE id = 2;
miniDB> SELECT * FROM users;
1 alice 31
```

---

## A few engineering decisions worth calling out

- **Per-table row size, computed from the real schema.** Row size used to be one shared, worst-case constant sized for the widest possible column. That silently collapsed every table's leaf-page capacity down to a single row per page. It's now computed per table from that table's actual catalog schema and threaded through every B+-tree accessor.
- **Query execution as a pull-based cursor pipeline.** A `PlanNode` tree (scan / PK-lookup / filter / projection) compiles from the AST once, then a matching `Cursor` tree pulls rows one at a time through a single dispatch point (`cursor_next`). No operator prints anything or knows what sits above or below it in the pipeline.
- **A real, traced bug chain, not a hypothetical one.** A structural assumption from Phase 4 (the root page always being page 0) silently broke once Phase 6 introduced a catalog page, and stayed dormant and harmless by coincidence for a long time. It only surfaced once splits became frequent again in Phase 7. Fixed by auditing every place that assumption was baked in, not just patching the crash site.

---

## Roadmap and known limitations

Phases 1–7, in-memory row store, persistent pages, single-node B+ tree, multi-level B+ tree, SQL lexer/AST, catalog and multiple tables, query execution pipeline, are complete. Phase 8 (Update and Delete) is mostly complete:

- Done: `UPDATE` / `DELETE` by primary key or by filter
- Done: deleted rows are correctly excluded; deleted space is reused for later inserts
- Not yet done: B+-tree rebalancing (merge/redistribution) after delete. The tree stays correct after deletes, it's just not space-efficient yet, sparse sibling leaves aren't merged back together. Designed, not yet implemented.
- Not started: Phase 9, file-format hardening (magic number, version, corruption detection)
- Not started: Phase 10, transactions (`BEGIN`/`COMMIT`/`ROLLBACK`) and crash recovery

---

## Further reading

Resources used while designing the B+-tree deletion/merging model:

- [Let's Build a Simple Database (cstack.github.io)](https://cstack.github.io/db_tutorial/parts/part7.html), the tutorial this project is loosely inspired by
- [USF B-Tree Visualization](https://www.cs.usfca.edu/~galles/visualization/BTree.html), interactive, watch the borrow-vs-merge decision happen step by step
- [CMU 15-445/645, Lecture 08: Tree Indexes](https://15445.courses.cs.cmu.edu/)
- Ramakrishnan & Gehrke, *Database Management Systems*, Chapter 10.6, "Deletion"
- Silberschatz, Korth & Sudarshan, *Database System Concepts*, B+-Tree deletion section
- Douglas Comer, ["The Ubiquitous B-Tree"](https://carlosproal.com/ir/papers/p121-comer.pdf) (1979)
- Goetz Graefe, ["Modern B-Tree Techniques"](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=0b19f413ffb5bc68b43f3bd05a97c282a7c6d6ab) (2011)
