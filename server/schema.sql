CREATE USER 'lucid' IDENTIFIED BY "hyperplane-kumquat-platypus"

CREATE DATABASE IF NOT EXISTS lucid;  
USE lucid;  
  
CREATE TABLE IF NOT EXISTS observers (  
    email VARCHAR(254) NOT NULL PRIMARY KEY,  
    pass  VARCHAR(254) NOT NULL,
    reset BOOL         NOT NULL DEFAULT FALSE
);  
  
CREATE TABLE IF NOT EXISTS administrators (  
    email VARCHAR(254) NOT NULL PRIMARY KEY,  
    pass  VARCHAR(254) NOT NULL,
    root  BOOL         NOT NULL DEFAULT FALSE
);  
  
CREATE TABLE IF NOT EXISTS clientlist (  
    hostname VARCHAR(253)      NOT NULL PRIMARY KEY,  
    nickname VARCHAR(32),  
    port     SMALLINT UNSIGNED NOT NULL DEFAULT 22,  
    rdp      SMALLINT UNSIGNED NOT NULL DEFAULT 3389
);

INSERT INTO administrators(email, pass, reset, root)
VALUES (
    "root@lucid.lucid",
    "$argon2id$v=19$m=65536,t=2,p=1$uCnApodcpUOwlyE8rdVTaw$zxU5DdYaHM0WH9hchVkwzIhU6Xno+T+4LF4s/fYfAU8",
    FALSE
);
INSERT INTO observers(email, pass, reset)
VALUES (
    "default@lucid.lucid",
    "$argon2id$v=19$m=65536,t=2,p=1$4paS77+977+977+9RHUoMGbvv73vv73vv70j6pWWzarvv71zSsy477+94paSZg$dsnJVqHLtVsGO50rqMtry4mu42zoTGBRHvlkCAvAPsk",
    FALSE
);

GRANT SELECT ON lucid.* TO lucid;
GRANT INSERT ON lucid.* TO lucid;
GRANT UPDATE ON lucid.* TO lucid;
GRANT DELETE ON lucid.* TO lucid;