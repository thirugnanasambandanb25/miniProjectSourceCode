// Bank-account program reads a random-access file sequentially,
// updates data already written to the file, creates new data to
// be placed in the file, and deletes data previously in the file.
//
// ENHANCEMENTS:
//   - Bug fixes: feof() misuse, missing fclose on error, fseek after fwrite
//   - Input validation throughout
//   - Search accounts by last name
//   - Sort & display all accounts by account number or balance
//   - Transaction history log (transactions.log)
//   - Interest / fee calculation

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_ACCOUNTS 100

// clientData structure definition
struct clientData {
    unsigned int acctNum;   // account number
    char lastName[15];      // account last name
    char firstName[10];     // account first name
    double balance;         // account balance
};

// ── Prototypes ────────────────────────────────────────────────────────────────
unsigned int enterChoice(void);
void         textFile(FILE *readPtr);
void         updateRecord(FILE *fPtr);
void         newRecord(FILE *fPtr);
void         deleteRecord(FILE *fPtr);
void         searchByName(FILE *fPtr);
void         sortAndDisplay(FILE *fPtr);
void         applyInterestOrFee(FILE *fPtr);
void         logTransaction(unsigned int acctNum, const char *type, double amount, double newBalance);

// ── Helpers ───────────────────────────────────────────────────────────────────

/* Safe integer read – flushes bad input and re-prompts until a valid
   value in [minVal, maxVal] is entered. */
static int safeReadInt(const char *prompt, int minVal, int maxVal)
{
    int value;
    while (1) {
        printf("%s", prompt);
        if (scanf("%d", &value) == 1 && value >= minVal && value <= maxVal) {
            // flush remainder of line
            int c; while ((c = getchar()) != '\n' && c != EOF);
            return value;
        }
        printf("  Invalid input. Please enter a number between %d and %d.\n",
               minVal, maxVal);
        int c; while ((c = getchar()) != '\n' && c != EOF); // flush bad input
    }
}

/* Safe double read – returns 1 on success, 0 on failure. */
static int safeReadDouble(const char *prompt, double *out)
{
    printf("%s", prompt);
    if (scanf("%lf", out) != 1) {
        int c; while ((c = getchar()) != '\n' && c != EOF);
        return 0;
    }
    int c; while ((c = getchar()) != '\n' && c != EOF);
    return 1;
}

/* Seek to the record for account number acctNum (1-based). */
static void seekToRecord(FILE *fPtr, unsigned int acctNum)
{
    fseek(fPtr, (long)(acctNum - 1) * (long)sizeof(struct clientData), SEEK_SET);
}

// ── main ──────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[])
{
    FILE *cfPtr;
    unsigned int choice;

    if ((cfPtr = fopen("credit.dat", "rb+")) == NULL) {
        fprintf(stderr, "%s: File 'credit.dat' could not be opened.\n",
                argc > 1 ? argv[0] : "bank");
        exit(EXIT_FAILURE);
    }

    while ((choice = enterChoice()) != 9) {
        switch (choice) {
            case 1: textFile(cfPtr);          break;
            case 2: updateRecord(cfPtr);      break;
            case 3: newRecord(cfPtr);         break;
            case 4: deleteRecord(cfPtr);      break;
            case 5: searchByName(cfPtr);      break;
            case 6: sortAndDisplay(cfPtr);    break;
            case 7: applyInterestOrFee(cfPtr);break;
            case 8:
                printf("Transaction log is saved to 'transactions.log'.\n");
                break;
            default:
                puts("Incorrect choice.");
                break;
        }
    }

    fclose(cfPtr);
    printf("Goodbye!\n");
    return EXIT_SUCCESS;
}

// ── 1. Text file dump ─────────────────────────────────────────────────────────
void textFile(FILE *readPtr)
{
    FILE *writePtr;
    struct clientData client = {0, "", "", 0.0};
    int result;

    if ((writePtr = fopen("accounts.txt", "w")) == NULL) {
        perror("accounts.txt could not be opened");
        return;
    }

    rewind(readPtr);
    fprintf(writePtr, "%-6s%-16s%-11s%10s\n", "Acct", "Last Name", "First Name", "Balance");

    // FIX: don't use feof() as the loop condition; read first, then check result
    while ((result = fread(&client, sizeof(struct clientData), 1, readPtr)) == 1) {
        if (client.acctNum != 0) {
            fprintf(writePtr, "%-6u%-16s%-11s%10.2f\n",
                    client.acctNum, client.lastName,
                    client.firstName, client.balance);
        }
    }

    fclose(writePtr);
    printf("accounts.txt written successfully.\n");
}

// ── 2. Update record ──────────────────────────────────────────────────────────
void updateRecord(FILE *fPtr)
{
    struct clientData client = {0, "", "", 0.0};
    double transaction;

    int account = safeReadInt("Enter account to update (1 - 100): ", 1, MAX_ACCOUNTS);

    seekToRecord(fPtr, (unsigned)account);
    if (fread(&client, sizeof(struct clientData), 1, fPtr) != 1 || client.acctNum == 0) {
        printf("Account #%d has no information.\n", account);
        return;
    }

    printf("\n%-6u%-16s%-11s%10.2f\n\n",
           client.acctNum, client.lastName, client.firstName, client.balance);

    if (!safeReadDouble("Enter charge (+) or payment (-): ", &transaction)) {
        puts("Invalid amount – update cancelled.");
        return;
    }

    client.balance += transaction;
    printf("Updated: %-6u%-16s%-11s%10.2f\n",
           client.acctNum, client.lastName, client.firstName, client.balance);

    // FIX: seek back before writing (position advanced by fread)
    seekToRecord(fPtr, (unsigned)account);
    fwrite(&client, sizeof(struct clientData), 1, fPtr);
    fflush(fPtr); // ensure write is committed

    logTransaction(client.acctNum,
                   transaction >= 0 ? "CHARGE" : "PAYMENT",
                   transaction, client.balance);
}

// ── 3. New record ─────────────────────────────────────────────────────────────
void newRecord(FILE *fPtr)
{
    struct clientData client = {0, "", "", 0.0};

    int accountNum = safeReadInt("Enter new account number (1 - 100): ", 1, MAX_ACCOUNTS);

    seekToRecord(fPtr, (unsigned)accountNum);
    if (fread(&client, sizeof(struct clientData), 1, fPtr) == 1 && client.acctNum != 0) {
        printf("Account #%d already contains information.\n", client.acctNum);
        return;
    }

    printf("Enter last name: ");
    if (scanf("%14s", client.lastName) != 1) { puts("Invalid input."); return; }
    printf("Enter first name: ");
    if (scanf("%9s", client.firstName) != 1) { puts("Invalid input."); return; }

    if (!safeReadDouble("Enter opening balance: ", &client.balance)) {
        puts("Invalid balance – account not created.");
        return;
    }
    if (client.balance < 0) {
        puts("Opening balance cannot be negative – account not created.");
        return;
    }

    client.acctNum = (unsigned)accountNum;
    seekToRecord(fPtr, (unsigned)accountNum);
    fwrite(&client, sizeof(struct clientData), 1, fPtr);
    fflush(fPtr);

    printf("Account #%d created.\n", accountNum);
    logTransaction(client.acctNum, "OPEN", client.balance, client.balance);
}

// ── 4. Delete record ──────────────────────────────────────────────────────────
void deleteRecord(FILE *fPtr)
{
    struct clientData client     = {0, "", "", 0.0};
    struct clientData blankClient = {0, "", "", 0.0};

    int accountNum = safeReadInt("Enter account number to delete (1 - 100): ", 1, MAX_ACCOUNTS);

    seekToRecord(fPtr, (unsigned)accountNum);
    if (fread(&client, sizeof(struct clientData), 1, fPtr) != 1 || client.acctNum == 0) {
        printf("Account %d does not exist.\n", accountNum);
        return;
    }

    // Confirm before deleting
    printf("Delete account #%d (%s %s, balance $%.2f)? (y/n): ",
           accountNum, client.firstName, client.lastName, client.balance);
    int c = getchar();
    int flush; while ((flush = getchar()) != '\n' && flush != EOF);
    if (tolower(c) != 'y') { puts("Deletion cancelled."); return; }

    seekToRecord(fPtr, (unsigned)accountNum);
    fwrite(&blankClient, sizeof(struct clientData), 1, fPtr);
    fflush(fPtr);

    printf("Account #%d deleted.\n", accountNum);
    logTransaction((unsigned)accountNum, "CLOSE", 0.0, 0.0);
}

// ── 5. Search by last name ────────────────────────────────────────────────────
void searchByName(FILE *fPtr)
{
    char searchName[15];
    struct clientData client = {0, "", "", 0.0};
    int found = 0;

    printf("Enter last name to search: ");
    if (scanf("%14s", searchName) != 1) { puts("Invalid input."); return; }
    // flush
    int c; while ((c = getchar()) != '\n' && c != EOF);

    // Case-insensitive compare helper (local)
    rewind(fPtr);
    printf("\n%-6s%-16s%-11s%10s\n", "Acct", "Last Name", "First Name", "Balance");
    printf("%-6s%-16s%-11s%10s\n", "----", "---------", "----------", "-------");

    while (fread(&client, sizeof(struct clientData), 1, fPtr) == 1) {
        if (client.acctNum == 0) continue;

        // case-insensitive comparison
        char lowerStored[15], lowerSearch[15];
        for (int i = 0; i < 15; i++)
            lowerStored[i] = (char)tolower((unsigned char)client.lastName[i]);
        for (int i = 0; i < 15; i++)
            lowerSearch[i] = (char)tolower((unsigned char)searchName[i]);

        if (strncmp(lowerStored, lowerSearch, 14) == 0) {
            printf("%-6u%-16s%-11s%10.2f\n",
                   client.acctNum, client.lastName,
                   client.firstName, client.balance);
            found++;
        }
    }

    if (!found)
        printf("No accounts found with last name '%s'.\n", searchName);
    else
        printf("\n%d account(s) found.\n", found);
}

// ── 6. Sort & display all accounts ───────────────────────────────────────────
void sortAndDisplay(FILE *fPtr)
{
    struct clientData accounts[MAX_ACCOUNTS];
    int count = 0;

    rewind(fPtr);
    struct clientData client;
    while (fread(&client, sizeof(struct clientData), 1, fPtr) == 1) {
        if (client.acctNum != 0)
            accounts[count++] = client;
    }

    if (count == 0) { puts("No accounts to display."); return; }

    int sortChoice = safeReadInt(
        "\nSort by:\n  1 - Account number\n  2 - Balance (ascending)\n  3 - Balance (descending)\n  4 - Last name\n? ",
        1, 4);

    // Bubble sort (small dataset – up to 100 records)
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            int swap = 0;
            switch (sortChoice) {
                case 1: swap = accounts[j].acctNum > accounts[j+1].acctNum; break;
                case 2: swap = accounts[j].balance  > accounts[j+1].balance; break;
                case 3: swap = accounts[j].balance  < accounts[j+1].balance; break;
                case 4: swap = strncmp(accounts[j].lastName, accounts[j+1].lastName, 14) > 0; break;
            }
            if (swap) {
                struct clientData tmp = accounts[j];
                accounts[j] = accounts[j+1];
                accounts[j+1] = tmp;
            }
        }
    }

    printf("\n%-6s%-16s%-11s%10s\n", "Acct", "Last Name", "First Name", "Balance");
    printf("%-6s%-16s%-11s%10s\n",   "----", "---------", "----------", "-------");

    double total = 0.0;
    for (int i = 0; i < count; i++) {
        printf("%-6u%-16s%-11s%10.2f\n",
               accounts[i].acctNum, accounts[i].lastName,
               accounts[i].firstName, accounts[i].balance);
        total += accounts[i].balance;
    }

    printf("%-6s%-16s%-11s%10s\n",   "----", "---------", "----------", "-------");
    printf("%-33s%10.2f\n", "Total balance:", total);
    printf("%-33s%10d\n",   "Total accounts:", count);
}

// ── 7. Apply interest or fee ──────────────────────────────────────────────────
void applyInterestOrFee(FILE *fPtr)
{
    int typeChoice = safeReadInt(
        "\n1 - Apply interest (%) to all accounts\n"
          "2 - Apply flat fee ($) to all accounts\n"
          "3 - Apply to a single account\n? ",
        1, 3);

    double rate = 0.0;
    int singleAcct = 0;

    if (typeChoice == 1) {
        if (!safeReadDouble("Enter annual interest rate (e.g. 3.5 for 3.5%): ", &rate)
            || rate < 0 || rate > 100) {
            puts("Invalid rate."); return;
        }
    } else if (typeChoice == 2) {
        if (!safeReadDouble("Enter fee amount ($): ", &rate) || rate < 0) {
            puts("Invalid fee."); return;
        }
    } else {
        singleAcct = safeReadInt("Enter account number (1 - 100): ", 1, MAX_ACCOUNTS);
        int subChoice = safeReadInt("1 - Interest (%)\n2 - Fee ($)\n? ", 1, 2);
        if (subChoice == 1) {
            if (!safeReadDouble("Enter rate (%): ", &rate) || rate < 0 || rate > 100) {
                puts("Invalid rate."); return;
            }
            typeChoice = 1; // reuse interest branch below
        } else {
            if (!safeReadDouble("Enter fee ($): ", &rate) || rate < 0) {
                puts("Invalid fee."); return;
            }
            typeChoice = 2;
        }
    }

    rewind(fPtr);
    struct clientData client;
    int updated = 0;
    long pos = 0;

    while (fread(&client, sizeof(struct clientData), 1, fPtr) == 1) {
        if (client.acctNum == 0) { pos++; continue; }
        if (singleAcct && (int)client.acctNum != singleAcct) { pos++; continue; }

        double oldBalance = client.balance;
        double change = 0.0;

        if (typeChoice == 1) {
            change = client.balance * (rate / 100.0);
            client.balance += change;
            logTransaction(client.acctNum, "INTEREST", change, client.balance);
        } else {
            change = -rate;
            client.balance -= rate;
            if (client.balance < 0) client.balance = 0.0; // floor at 0
            logTransaction(client.acctNum, "FEE", -rate, client.balance);
        }

        seekToRecord(fPtr, client.acctNum);
        fwrite(&client, sizeof(struct clientData), 1, fPtr);
        fflush(fPtr);

        printf("Acct #%-4u: $%8.2f -> $%8.2f  (change: %+.2f)\n",
               client.acctNum, oldBalance, client.balance, change);
        updated++;

        // restore read position
        fseek(fPtr, (long)(pos + 1) * (long)sizeof(struct clientData), SEEK_SET);
        pos++;
    }

    printf("\n%d account(s) updated.\n", updated);
}

// ── Transaction logger ────────────────────────────────────────────────────────
void logTransaction(unsigned int acctNum, const char *type, double amount, double newBalance)
{
    FILE *logPtr = fopen("transactions.log", "a");
    if (!logPtr) return; // non-fatal – just skip logging

    time_t now = time(NULL);
    char timeBuf[20];
    struct tm *t = localtime(&now);
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", t);

    fprintf(logPtr, "%s | Acct#%-4u | %-10s | Amount: %+10.2f | Balance: %10.2f\n",
            timeBuf, acctNum, type, amount, newBalance);

    fclose(logPtr);
}

// ── Menu ──────────────────────────────────────────────────────────────────────
unsigned int enterChoice(void)
{
    printf("         BANK ACCOUNT MANAGER         \n");
    printf("\n");
    printf("  1 - Export accounts to text file    \n");
    printf("  2 - Update an account               \n");
    printf("  3 - Add a new account               \n");
    printf("  4 - Delete an account               \n");
    printf("  5 - Search accounts by last name    \n");
    printf("  6 - Sort and display all accounts   \n");
    printf("  7 - Apply interest / fee            \n");
    printf("  8 - View transaction log info       \n");
    printf("  9 - Exit                            \n");
    printf("\n");
    printf("? ");

    unsigned int choice;
    if (scanf("%u", &choice) != 1) {
        int c; while ((c = getchar()) != '\n' && c != EOF);
        return 0; // will hit default in switch
    }
    int c; while ((c = getchar()) != '\n' && c != EOF);
    return choice;
}