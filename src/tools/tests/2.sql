CREATE TABLE IF NOT EXISTS  "Suppliers" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "name" VARCHAR(30) NOT NULL
);
CREATE TABLE IF NOT EXISTS  "Account" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "iban" VARCHAR(30) NOT NULL,
    "supplier_id" BIGINT
);
CREATE TABLE IF NOT EXISTS "AccountHistory" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "credit_rating" INTEGER NOT NULL,
    "account_id" BIGINT
);
