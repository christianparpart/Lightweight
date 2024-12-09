CREATE TABLE IF NOT EXISTS User (
    id              INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    fullname        VARCHAR(128) NOT NULL,
    email           VARCHAR(60) NOT NULL
);
CREATE TABLE IF NOT EXISTS TaskList (
    id              INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    user_id         INT NOT NULL,
    CONSTRAINT      fk1 FOREIGN KEY (user_id) REFERENCES user(id)
);
CREATE TABLE IF NOT EXISTS TaskListEntry (
    id              INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    tasklist_id     INT NOT NULL,
    completed       DATETIME NULL,
    task            VARCHAR(255) NOT NULL,
    CONSTRAINT      fk1 FOREIGN KEY (tasklist_id) REFERENCES TaskList(id)
);
