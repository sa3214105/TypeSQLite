# TODO List

## Missing SQLite Features to Implement

### 1. View Support (視圖支援)
- [ ] CREATE VIEW
- [ ] DROP VIEW
- [ ] Query from views

### 2. Trigger Support (觸發器支援)
- [ ] CREATE TRIGGER
- [ ] DROP TRIGGER
- [ ] BEFORE/AFTER/INSTEAD OF triggers
- [ ] INSERT/UPDATE/DELETE trigger events

### 3. ALTER TABLE Support (表結構修改)
- [ ] ALTER TABLE RENAME TO
- [ ] ALTER TABLE ADD COLUMN
- [ ] ALTER TABLE DROP COLUMN
- [ ] ALTER TABLE RENAME COLUMN

### 4. PRAGMA Statements (資料庫設定)
- [ ] PRAGMA foreign_keys
- [ ] PRAGMA journal_mode
- [ ] PRAGMA synchronous
- [ ] PRAGMA cache_size
- [ ] PRAGMA temp_store
- [ ] PRAGMA user_version
- [ ] PRAGMA table_info

### 5. Common Table Expressions (CTE) (公用資料表運算式)
- [ ] WITH clause (non-recursive)
- [ ] WITH RECURSIVE clause
- [ ] Multiple CTEs in single query

### 6. CASE Expression (條件運算式)
- [ ] CASE WHEN ... THEN ... ELSE ... END
- [ ] Simple CASE expression
- [ ] Searched CASE expression

### 7. Subquery Support (子查詢)
- [ ] Scalar subqueries
- [ ] IN subquery
- [ ] EXISTS subquery
- [ ] Correlated subqueries

### 8. CAST and Type Conversion (型別轉換)
- [x] CAST(expr AS type) ✅ (implemented in Expressions.hpp)
- [ ] Type affinity functions

### 9. Date and Time Functions (日期時間函數) ✅ **COMPLETED**
- [x] datetime() ✅
- [x] date() ✅
- [x] time() ✅
- [x] strftime() ✅
- [x] julianday() ✅
- [x] unixepoch() ✅
- [x] timediff() ✅
- [x] Date arithmetic (via modifiers) ✅

### 10. String Functions (字串函數) ✅ **COMPLETED**
- [x] SUBSTR/SUBSTRING ✅
- [x] CONCAT ✅
- [x] CONCAT_WS ✅
- [x] TRIM/LTRIM/RTRIM ✅
- [x] UPPER/LOWER ✅
- [x] LENGTH ✅
- [x] REPLACE ✅
- [x] LIKE/GLOB pattern matching ✅
- [x] INSTR ✅
- [x] printf() ✅
- [x] format() ✅
- [x] char() ✅
- [x] hex() ✅
- [x] unhex() ✅
- [x] quote() ✅
- [x] soundex() ✅
- [x] unicode() ✅
- [x] unistr() ✅

### 11. Math Functions (數學函數) ✅ **COMPLETED**
- [x] ABS ✅
- [x] ROUND ✅
- [x] CEIL/CEILING ✅
- [x] FLOOR ✅
- [x] RANDOM ✅
- [x] RANDOMBLOB ✅
- [x] POWER ✅
- [x] SQRT ✅
- [x] MOD ✅
- [x] SIN/COS/TAN ✅
- [x] ASIN/ACOS/ATAN/ATAN2 ✅
- [x] SINH/COSH/TANH ✅
- [x] ASINH/ACOSH/ATANH ✅
- [x] EXP/LN/LOG/LOG10/LOG2 ✅
- [x] PI ✅
- [x] DEGREES/RADIANS ✅
- [x] SIGN ✅
- [x] TRUNC ✅

### 12. NULL Handling (NULL 處理) ✅ **COMPLETED**
- [x] COALESCE ✅
- [x] IFNULL ✅
- [x] NULLIF ✅
- [x] IS NULL / IS NOT NULL operators ✅

### 13. GROUP BY Enhancements (分組增強功能)
- [x] HAVING clause ✅
- [x] GROUP_CONCAT function ✅
- [ ] GROUPING SETS (if supported by SQLite version)

### 14. ORDER BY Enhancements (排序增強功能)
- [x] Multiple sort keys ✅
- [ ] NULLS FIRST / NULLS LAST
- [ ] COLLATE clause

### 15. UNION Operations (聯集操作)
- [ ] UNION
- [ ] UNION ALL
- [ ] INTERSECT
- [ ] EXCEPT

### 16. Prepared Statement Optimization (預處理語句優化)
- [x] Statement caching (basic implementation) ✅
- [ ] Batch execution optimization

### 17. ATTACH/DETACH Database (附加/分離資料庫)
- [ ] ATTACH DATABASE
- [ ] DETACH DATABASE
- [ ] Cross-database queries

### 18. Backup and Restore (備份與還原)
- [ ] Database backup API
- [ ] Export to SQL
- [ ] Import from SQL

### 19. User-Defined Functions (使用者自訂函數)
- [ ] Register custom scalar functions
- [ ] Register custom aggregate functions
- [ ] Register custom collation sequences

### 20. JSON Support (JSON 支援) 
- [ ] JSON1 extension functions
- [ ] json()
- [ ] json_extract()
- [ ] json_array()
- [ ] json_object()
- [ ] json_insert()
- [ ] json_replace()
- [ ] json_set()
- [ ] json_remove()
- [ ] json_patch()
- [ ] json_type()
- [ ] json_valid()
- [ ] json_quote()
- [ ] json_array_length()

### 21. Full-Text Search (全文搜尋)
- [ ] FTS5 table creation
- [ ] MATCH operator
- [ ] FTS5 auxiliary functions
- [ ] Ranking and snippets

### 22. Conflict Resolution (衝突解決)
- [x] ON CONFLICT REPLACE (via UPSERT) ✅
- [ ] ON CONFLICT ROLLBACK
- [ ] ON CONFLICT ABORT
- [ ] ON CONFLICT FAIL
- [ ] ON CONFLICT IGNORE

### 23. VACUUM and Database Maintenance (資料庫維護)
- [ ] VACUUM
- [ ] VACUUM INTO
- [ ] ANALYZE
- [ ] REINDEX

### 24. Savepoint Support (儲存點支援)
- [ ] SAVEPOINT
- [ ] RELEASE SAVEPOINT
- [ ] ROLLBACK TO SAVEPOINT

### 25. Schema Introspection (結構描述檢查)
- [ ] List all tables
- [ ] Get table schema
- [ ] List all indexes
- [ ] Get index info

### 26. EXPLAIN Query Plan (查詢計畫說明)
- [ ] EXPLAIN
- [ ] EXPLAIN QUERY PLAN

### 27. Blob Operations (二進位大型物件操作)
- [x] Blob binding and retrieval (basic) ✅
- [ ] Incremental Blob I/O

### 28. WHERE Clause Enhancements (WHERE 子句增強)
- [x] IN operator with list ✅
- [x] BETWEEN operator ✅
- [x] GLOB operator ✅
- [x] REGEXP operator (if enabled) ✅
- [x] MATCH operator ✅

### 29. Safety Features (安全功能)
- [x] Force WHERE clause for UPDATE (需要 WHERE 才能執行) ✅
- [x] Force WHERE clause for DELETE (需要 WHERE 才能執行) ✅
- [ ] Query timeout
- [ ] Result row limit
- [ ] Dry-run mode

### 30. Performance Optimization (效能優化)
- [ ] Connection pooling
- [ ] Lazy loading
- [ ] Query result streaming
- [ ] Memory-mapped I/O

### 31 Other KeyWords (其他關鍵字)
- [x] CURRENT_DATE ✅
- [x] CURRENT_TIME ✅
- [x] CURRENT_TIMESTAMP ✅

### 32. Scalar Functions (標量函數) ✅ **COMPLETED**
- [x] All 60+ SQLite core scalar functions ✅
- [x] changes() ✅
- [x] total_changes() ✅
- [x] last_insert_rowid() ✅
- [x] sqlite_version() ✅
- [x] sqlite_source_id() ✅
- [x] sqlite_compileoption_get() ✅
- [x] sqlite_compileoption_used() ✅
- [x] sqlite_offset() ✅
- [x] zeroblob() ✅
- [x] typeof() ✅
- [x] octet_length() ✅
- [x] likelihood()/likely()/unlikely() ✅
- [x] load_extension() ✅
- [x] iif() / if() ✅

---

## Recently Completed Features

### Core Database Operations
- ✅ Basic CRUD operations (Insert/Select/Update/Delete)
- ✅ Transaction support with automatic commit/rollback
- ✅ Batch insert operations
- ✅ UPSERT (INSERT OR REPLACE)

### Query Features
- ✅ Join operations (INNER/LEFT/RIGHT/FULL/CROSS)
- ✅ DISTINCT queries
- ✅ LIMIT/OFFSET pagination
- ✅ Expression support (comparison operators, arithmetic)
- ✅ WHERE clause (with BETWEEN, IN, GLOB, REGEXP, MATCH)

### Aggregate & Window Functions
- ✅ Aggregate functions (COUNT/SUM/AVG/MIN/MAX/GROUP_CONCAT)
- ✅ Window functions (ROW_NUMBER, RANK, DENSE_RANK, NTILE, LAG, LEAD, FIRST_VALUE, LAST_VALUE, NTH_VALUE)

### Mathematical Functions (完整實現)
- ✅ Basic math (ABS, ROUND, CEIL, FLOOR, TRUNC, SIGN, MOD)
- ✅ Power & roots (POWER, SQRT)
- ✅ Trigonometric functions (SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2)
- ✅ Hyperbolic functions (SINH, COSH, TANH, ASINH, ACOSH, ATANH)
- ✅ Exponential & logarithmic (EXP, LN, LOG, LOG10, LOG2)
- ✅ Constants & conversion (PI, DEGREES, RADIANS)
- ✅ Random numbers (RANDOM, RANDOMBLOB)

### String Functions (完整實現)
- ✅ Case conversion (UPPER, LOWER)
- ✅ Trimming (TRIM, LTRIM, RTRIM)
- ✅ Substring operations (SUBSTR, SUBSTRING)
- ✅ Concatenation (CONCAT, CONCAT_WS, || operator)
- ✅ String info (LENGTH, OCTET_LENGTH, INSTR)
- ✅ Replacement (REPLACE)
- ✅ Pattern matching (LIKE, GLOB)
- ✅ Formatting (PRINTF, FORMAT)
- ✅ Encoding (HEX, UNHEX, CHAR, UNICODE, UNISTR, UNISTR_QUOTE)
- ✅ Phonetic (SOUNDEX)
- ✅ Quoting (QUOTE)

### Date & Time Functions (完整實現)
- ✅ Date extraction (DATE, TIME, DATETIME)
- ✅ Formatting (STRFTIME)
- ✅ Conversions (JULIANDAY, UNIXEPOCH)
- ✅ Time difference (TIMEDIFF)
- ✅ Date arithmetic (via modifiers: +N days/hours/etc.)

### Scalar Functions (完整實現)
- ✅ All 60+ SQLite core scalar functions
- ✅ NULL handling (COALESCE, IFNULL, NULLIF)
- ✅ Conditional logic (IIF, IF)
- ✅ Type checking & conversion (TYPEOF, CAST)
- ✅ SQLite metadata (VERSION, SOURCE_ID, COMPILEOPTION_GET/USED, OFFSET)
- ✅ Row tracking (CHANGES, TOTAL_CHANGES, LAST_INSERT_ROWID)
- ✅ Query optimization hints (LIKELIHOOD, LIKELY, UNLIKELY)
- ✅ Binary data (ZEROBLOB, RANDOMBLOB)
- ✅ Extension loading (LOAD_EXTENSION)

### Schema & Constraints
- ✅ Column constraints (PRIMARY KEY/NOT NULL/UNIQUE/DEFAULT/CHECK)
- ✅ Table constraints (FOREIGN KEY, UNIQUE, CHECK)
- ✅ Index support (CREATE/DROP INDEX, UNIQUE indexes)
- ✅ Table options (WITHOUT ROWID, STRICT)

### Safety Features
- ✅ Force WHERE for UPDATE/DELETE safety
- ✅ Type-safe query building
- ✅ Compile-time SQL validation

---

## Priority (優先順序)

### High Priority (高優先)
1. ✅ ~~String functions~~ **COMPLETED**
2. ✅ ~~Math functions~~ **COMPLETED**
3. ✅ ~~Date/Time functions~~ **COMPLETED**
4. ✅ ~~NULL handling functions~~ **COMPLETED**
5. Subquery support (needed for complex queries)
6. CASE expressions (common in business logic)
7. UNION operations

### Medium Priority (中優先)
8. ALTER TABLE support
9. View support
10. CTE (Common Table Expressions)
11. SAVEPOINT support
12. Schema introspection

### Low Priority (低優先)
13. Trigger support
14. Full-text search (FTS5)
15. User-defined functions
16. ATTACH/DETACH
17. VACUUM and maintenance
18. Json support

### Nice to Have (可選功能)
19. Backup/Restore utilities
20. Query plan explanation (EXPLAIN)
21. Connection pooling
22. Advanced optimizations
23. Incremental Blob I/O

---

## Implementation Notes

### Module Organization
- **Core**: SQLiteWrapper.hpp, Database.hpp, Table.hpp
- **Query Building**: SelectAble.hpp, DataSource.hpp
- **Expressions**: Expressions.hpp, AggregateFunctions.hpp, WindowFunctions.hpp
- **Math**: MathFunctions.hpp (complete)
- **Strings**: ScalarFunctions.hpp (string functions)
- **DateTime**: DateTimeFunctions.hpp (complete)
- **Constraints**: ColumnConstraints.hpp, TableConstraint.hpp
- **Indexes**: Index.hpp

### Test Coverage
- ✅ Comprehensive unit tests for all implemented functions
- ✅ Integration tests for complex queries
- ✅ Safety feature tests (forced WHERE clauses)
- 57+ scalar function tests
- 22+ datetime function tests
- 11+ math function tests
- 17+ window function tests


