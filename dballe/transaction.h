#ifndef DBALLE_TRANSACTION_H
#define DBALLE_TRANSACTION_H

namespace dballe {

/**
 * A RAII transaction interface.
 *
 * The transaction will be valid during the lifetime of this object.
 *
 * You can commit or rollback the transaction using its methods. If at
 * destruction time the transaction has not been committed or rolled back, a
 * rollback is automatically performed.
 */
class Transaction
{
public:
    virtual ~Transaction() {}

    /// Commit this transaction
    virtual void commit() = 0;

    /// Roll back this transaction
    virtual void rollback() = 0;

    /// Roll back this transaction
    virtual void rollback_nothrow() noexcept = 0;
};

}

#endif
