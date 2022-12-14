#include "blockchain.hpp"
#include <iostream>
#include "defines.hpp"
#include "omp.h"

void Blockchain::addTransactionToNewBlock(
    std::vector<User>& us,
    std::vector<Transaction>& poo,
    std::vector<Transaction>& invalid,
    std::vector<Transaction>& candidates) {
    do {
        // check if pool is empty before working with it
        if (poo.empty()) {
#ifdef VERBOSE_ADD
            std::cout << "No transactions left in a pool\n";
#endif
            return;
        }
        auto it = selectRandomTransaction(poo);
        auto sender = findUserByPk(us, it->getSender());

        // check the amount transferred
        if (sender->getBalance() < it->getAmount()) {
#ifdef VERBOSE_ADD
            std::cout << "Sender has not enough balance for the "
                         "transaction. \nBalance: "
                      << sender->getBalance()
                      << "\nTransaction amount: " << it->getAmount()
                      << std::endl;
#endif
            invalid.push_back(*it);
            poo.erase(it);
            continue;
        }

        // add chosen transaction to the candidates
        candidates.push_back(*it);
        // update user balance
        updateUserBalance(us, *it);
        // transaction found, finish execution
        poo.erase(it);
        return;
    } while (true);
}

void Blockchain::generateTransactions(const int& count,
                                      const int& min,
                                      const int& max) {
    // there should more than 1 user to generate transactions
    if (users.size() < 2) {
        std::cout
            << "Not enough users to generate transactions. There is only: "
            << users.size() << " users.\n";
        return;
    }

    for (int i = 0; i < count; i++) {
        int num1 = generateRandomNumber(0, users.size() - 1);
        auto sender = findUserByUsername(users, "user" + std::to_string(num1));

        // transaction cant have the same sender and address
        int num2;
        do {
            num2 = generateRandomNumber(0, users.size() - 1);
        } while (num2 == num1);

        auto address = findUserByUsername(users, "user" + std::to_string(num2));
        auto transactionAmount = generateRandomNumber(min, max);

        pool.emplace_back(sender->getPublicKey(), address->getPublicKey(),
                          transactionAmount);
#ifdef VERBOSE_GENERATION
        std::cout << pool.back() << std::endl;
#endif
    }
}

void Blockchain::mineBlock(const size_t& initialBlockchainSize,
                           bool& finishedMining) {
    // sanity check
    if (users.empty()) {
        std::cerr << "Create the users." << std::endl;
        return;
    }
#ifdef VERBOSE_MINING
    std::cout << std::string(50, '-') << "\nMining the block "
              << blockchain.size() << std::endl;
#endif
    // create a vector for potential transactions
    std::vector<Transaction> candidates;
    // temporary vectors to store effects of a new block
    std::vector<Transaction> tempPool(pool);
    std::vector<Transaction> invalidTxs;
    std::vector<User> tempUsers(users);
    // immediately return if pool is empty
    if (tempPool.empty()) {
#ifdef VERBOSE_MINING
        std::cout << "The pool is empty. Nothing to mine." << std::endl;
#endif
        return;
    }

#ifdef VERBOSE_MINING
    std::cout << "Adding transactions..." << std::endl;
#endif
    // add new transactions to a block while there still are
    // transactions or we reached the maximum amount of transactions in
    // a block
    while (!tempPool.empty() && candidates.size() < TRANSACTIONS_IN_BLOCK) {
        addTransactionToNewBlock(tempUsers, tempPool, invalidTxs, candidates);
    }

    // we cant create a new block if there are no transactions
#pragma omp critical
    if (candidates.empty()) {
        // remove transaction from a real pool
        // if we go to here we know that there are no valid transactions inside
        // temp pool and we can safely clear the real pool
        pool.clear();
#ifdef VERBOSE_MINING
        std::cout << "There is not enough transactions to form a block.\n";
#endif
    }

    if (candidates.empty() && pool.empty())
        return;

    // create a new block
    Block block(getLastBlockHash(), DIFFICULTY_TARGET, 1, candidates);

    // actually mine it
    block.mine(finishedMining);

#pragma omp flush(finishedMining)
    if (finishedMining)
        return;

    // check newly mined block hash
    if (!checkBlockHash(block)) {
        std::cerr << "Tampering detected. Block hash is invalid.\n";
        return;
    }

    // DANGER ZONE FOR OPENMP
    // only one thread at a time can access this
#pragma omp critical
    if (blockchain.size() == initialBlockchainSize) {
        // set a flag for other openmp threads
        finishedMining = true;

        // add the mined block to the blockhain
        blockchain.push_back(block);

        // update users' balance
        for (const auto& t : block.getTransactions())
            updateUserBalance(users, t);

        // remove transactions from a pool
        removeTransactions(block.getTransactions(),
                           "Removing transactions from a pool.");

        // remove invalid transactions from a pool
        if (!invalidTxs.empty())
            removeTransactions(invalidTxs,
                               "Removing invalid transactions from a pool.");

        std::cout << "Block " << blockchain.size() - 1
                  << " has been mined by thread " << omp_get_thread_num()
                  << "\n";
    }
}

void Blockchain::removeTransactions(const std::vector<Transaction>& txs,
                                    const std::string& msg) {
#ifdef VERBOSE_REMOVE
    std::cout << msg << std::endl;
#endif
    for (const auto& t : txs) {
        auto it = std::find_if(pool.begin(), pool.end(), [&t](Transaction tr) {
            return tr.getId() == t.getId();
        });
        pool.erase(it);
    }
}
