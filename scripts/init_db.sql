CREATE DATABASE IF NOT EXISTS secmng DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE secmng;

CREATE TABLE IF NOT EXISTS keysn (
  ikeysn INT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO keysn (ikeysn)
SELECT 1
WHERE NOT EXISTS (SELECT 1 FROM keysn);

CREATE TABLE IF NOT EXISTS seckeyinfo (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  clientid VARCHAR(64) NOT NULL,
  serverid VARCHAR(64) NOT NULL,
  keyid INT NOT NULL,
  createtime DATETIME NOT NULL,
  state TINYINT NOT NULL DEFAULT 1,
  seckey VARCHAR(512) NOT NULL,
  PRIMARY KEY (id),
  UNIQUE KEY uk_client_server_key (clientid, serverid, keyid),
  KEY idx_client_server_state (clientid, serverid, state)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS message_log (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  msg_id VARCHAR(128) NOT NULL,
  sender_id VARCHAR(64) NOT NULL,
  receiver_id VARCHAR(64) NOT NULL,
  sender_key_id INT NOT NULL,
  receiver_key_id INT NOT NULL,
  msg_type VARCHAR(32) NOT NULL,
  sender_ciphertext TEXT NOT NULL,
  sender_nonce VARCHAR(256) NOT NULL,
  sender_tag VARCHAR(256) NOT NULL,
  receiver_ciphertext TEXT NOT NULL,
  receiver_nonce VARCHAR(256) NOT NULL,
  receiver_tag VARCHAR(256) NOT NULL,
  algorithm VARCHAR(64) NOT NULL,
  send_time DATETIME NOT NULL,
  status TINYINT NOT NULL DEFAULT 1,
  PRIMARY KEY (id),
  KEY idx_msg_id (msg_id),
  KEY idx_sender_time (sender_id, send_time),
  KEY idx_receiver_time (receiver_id, send_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS audit_log (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  log_id VARCHAR(128) NOT NULL,
  node_id VARCHAR(64) NULL,
  action VARCHAR(64) NOT NULL,
  target_id VARCHAR(128) NOT NULL,
  result TINYINT NOT NULL,
  detail VARCHAR(1024) NOT NULL,
  create_time DATETIME NOT NULL,
  PRIMARY KEY (id),
  KEY idx_log_id (log_id),
  KEY idx_node_time (node_id, create_time),
  KEY idx_action_time (action, create_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
