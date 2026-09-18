-- Player currency balances. Currency values are account-owned and never keyed by device.
CREATE TABLE IF NOT EXISTS account_currencies (
    account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    currency TEXT NOT NULL,
    amount INTEGER NOT NULL DEFAULT 0 CHECK(amount >= 0),
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(account_id, currency)
);

CREATE INDEX IF NOT EXISTS account_currencies_account_idx
    ON account_currencies(account_id);
