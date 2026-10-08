#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

// ============================================================================
// KONFIGURASI (asumsi simulasi)
// ============================================================================
#define KODE_USSD "*858#"
#define MIN_TRANSFER 5000       // nominal minimum (Rp)
#define MAKS_TRANSFER 100000    // nominal maksimum per transaksi (Rp)
#define KELIPATAN 1000          // nominal harus kelipatan ini
#define BIAYA_ADMIN 1500        // biaya admin dibebankan ke pengirim (Rp)
#define MIN_SISA_SALDO 0        // sisa saldo minimum setelah transfer (Rp)

#define MAX_PELANGGAN 50
#define MAX_TRANSAKSI 100
#define LENGTH_NOMOR 20
#define LENGTH_WAKTU 30

const char *MENU_UTAMA =
    "Layanan *858#\n"
    "1.Transfer Pulsa\n"
    "2.Minta Pulsa\n"
    "3.Auto TP\n"
    "4.Delete Auto TP\n"
    "5.List Auto TP\n"
    "6.Cek Kupon Undian TP";

const char *PROMPT_NOMOR = "Silahkan masukkan nomor tujuan Transfer Pulsa : (contoh: 08xxxx atau 628xxxx)";

// ============================================================================
// DATA STRUCTURES (pengganti database)
// ============================================================================
typedef struct {
    char nomor[LENGTH_NOMOR];
    int saldo;
    bool aktif;
} Pelanggan;

typedef struct {
    char waktu[LENGTH_WAKTU];
    char pengirim[LENGTH_NOMOR];
    char penerima[LENGTH_NOMOR];
    int nominal;
    int biaya;
    char status[10]; // "SUKSES" atau "GAGAL"
} Transaksi;

typedef struct {
    Pelanggan pelanggan[MAX_PELANGGAN];
    int jumlah_pelanggan;
    Transaksi transaksi[MAX_TRANSAKSI];
    int jumlah_transaksi;
} Database;

// Helper: Tambah pelanggan ke Database
void tambah_pelanggan(Database *db, const char *nomor, int saldo, bool aktif) {
    if (db->jumlah_pelanggan < MAX_PELANGGAN) {
        strcpy(db->pelanggan[db->jumlah_pelanggan].nomor, nomor);
        db->pelanggan[db->jumlah_pelanggan].saldo = saldo;
        db->pelanggan[db->jumlah_pelanggan].aktif = aktif;
        db->jumlah_pelanggan++;
    }
}

// Helper: Cari pelanggan berdasarkan nomor
Pelanggan* cari_pelanggan(Database *db, const char *nomor) {
    for (int i = 0; i < db->jumlah_pelanggan; i++) {
        if (strcmp(db->pelanggan[i].nomor, nomor) == 0) {
            return &db->pelanggan[i];
        }
    }
    return NULL;
}

// ============================================================================
// PROSES 2.1: Validasi Nomor Tujuan
// ============================================================================
bool normalisasi_nomor(const char *teks, char *output) {
    char cleaned[100] = "";
    int idx = 0;

    // Bersihkan spasi dan strip
    for (int i = 0; teks[i] != '\0'; i++) {
        if (teks[i] != ' ' && teks[i] != '-' && teks[i] != '\n' && teks[i] != '\r') {
            cleaned[idx++] = teks[i];
        }
    }
    cleaned[idx] = '\0';

    char *p = cleaned;
    if (p[0] == '+') p++;

    // Cek apakah semua karakter berupa angka
    for (int i = 0; p[i] != '\0'; i++) {
        if (!isdigit(p[i])) return false;
    }

    // Ubah awalan 62 menjadi 0
    if (strncmp(p, "62", 2) == 0) {
        output[0] = '0';
        strcpy(output + 1, p + 2);
    } else {
        strcpy(output, p);
    }

    // Validasi panjang dan awalan 08 (total 10-13 digit)
    size_t len = strlen(output);
    if (strncmp(output, "08", 2) == 0 && len >= 10 && len <= 13) {
        return true;
    }

    return false;
}

bool validasi_nomor_tujuan(Database *db, const char *teks, const char *nomor_pengirim, char *nomor_out, char *pesan_error) {
    if (!normalisasi_nomor(teks, nomor_out)) {
        strcpy(pesan_error, "Format nomor tidak valid. Gunakan 08xxxx atau 628xxxx.");
        return false;
    }
    if (strcmp(nomor_out, nomor_pengirim) == 0) {
        strcpy(pesan_error, "Tidak dapat transfer pulsa ke nomor sendiri.");
        return false;
    }

    Pelanggan *penerima = cari_pelanggan(db, nomor_out);
    if (penerima == NULL || !penerima->aktif) {
        strcpy(pesan_error, "Nomor tujuan tidak terdaftar / tidak aktif.");
        return false;
    }

    return true;
}

// ============================================================================
// PROSES 2.2: Validasi Nominal dan Saldo
// ============================================================================
bool validasi_nominal(Database *db, const char *teks, const char *nomor_pengirim, int *nominal_out, char *pesan_error) {
    char cleaned[50] = "";
    int idx = 0;

    // Bersihkan titik, spasi, dan newline
    for (int i = 0; teks[i] != '\0'; i++) {
        if (teks[i] != '.' && teks[i] != ' ' && teks[i] != '\n' && teks[i] != '\r') {
            if (!isdigit(teks[i])) {
                strcpy(pesan_error, "Nominal harus berupa angka.");
                return false;
            }
            cleaned[idx++] = teks[i];
        }
    }
    cleaned[idx] = '\0';

    if (strlen(cleaned) == 0) {
        strcpy(pesan_error, "Nominal harus berupa angka.");
        return false;
    }

    int nominal = atoi(cleaned);

    if (nominal < MIN_TRANSFER || nominal > MAKS_TRANSFER) {
        sprintf(pesan_error, "Nominal harus antara Rp%d dan Rp%d.", MIN_TRANSFER, MAKS_TRANSFER);
        return false;
    }
    if (nominal % KELIPATAN != 0) {
        sprintf(pesan_error, "Nominal harus kelipatan Rp%d.", KELIPATAN);
        return false;
    }

    Pelanggan *pengirim = cari_pelanggan(db, nomor_pengirim);
    if (pengirim == NULL || (pengirim->saldo - (nominal + BIAYA_ADMIN) < MIN_SISA_SALDO)) {
        strcpy(pesan_error, "Pulsa Anda tidak mencukupi untuk transaksi ini.");
        return false;
    }

    *nominal_out = nominal;
    return true;
}

// ============================================================================
// PROSES 2.3: Konfirmasi Transaksi
// ============================================================================
void cetak_konfirmasi(const char *nomor_tujuan, int nominal) {
    printf("Transfer Pulsa Rp%d ke %s.\n", nominal, nomor_tujuan);
    printf("Biaya admin Rp%d. Total Rp%d.\n", BIAYA_ADMIN, nominal + BIAYA_ADMIN);
    printf("1.Ya\n2.Tidak\n");
}

// ============================================================================
// PROSES 2.4: Eksekusi Transfer
// ============================================================================
Transaksi eksekusi_transfer(Database *db, const char *pengirim, const char *penerima, int nominal) {
    int total = nominal + BIAYA_ADMIN;
    Pelanggan *p_kirim = cari_pelanggan(db, pengirim);
    Pelanggan *p_terima = cari_pelanggan(db, penerima);

    Transaksi trx;

    // Dapatkan waktu saat ini
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(trx.waktu, sizeof(trx.waktu), "%Y-%m-%d %H:%M:%S", t);

    strcpy(trx.pengirim, pengirim);
    strcpy(trx.penerima, penerima);
    trx.nominal = nominal;
    trx.biaya = BIAYA_ADMIN;

    // Cek ulang saldo sebelum eksekusi
    if (p_kirim == NULL || (p_kirim->saldo - total < MIN_SISA_SALDO)) {
        strcpy(trx.status, "GAGAL");
    } else {
        p_kirim->saldo -= total;
        if (p_terima != NULL) {
            p_terima->saldo += nominal;
        }
        strcpy(trx.status, "SUKSES");
    }

    if (db->jumlah_transaksi < MAX_TRANSAKSI) {
        db->transaksi[db->jumlah_transaksi++] = trx;
    }

    return trx;
}

// ============================================================================
// PROSES 2.5: Kirim Notifikasi
// ============================================================================
void notifikasi(Database *db, Transaksi trx) {
    if (strcmp(trx.status, "SUKSES") != 0) {
        printf("Transfer pulsa gagal. Silakan coba lagi.\n");
        return;
    }

    Pelanggan *p_kirim = cari_pelanggan(db, trx.pengirim);
    int sisa = (p_kirim != NULL) ? p_kirim->saldo : 0;

    printf("Transfer pulsa Rp%d ke %s BERHASIL. Sisa pulsa Rp%d.\n", trx.nominal, trx.penerima, sisa);
    printf("[SMS ke %s] Anda menerima pulsa Rp%d dari %s.\n", trx.penerima, trx.nominal, trx.pengirim);
}

// ============================================================================
// PROSES 1.0: Sesi USSD (menu -> pilihan -> sampai eksekusi)
// ============================================================================
void alur_transfer_pulsa(Database *db, const char *pengirim) {
    char input_buf[100];
    char nomor_tujuan[LENGTH_NOMOR];
    char pesan_error[150];
    int nominal = 0;

    // Level 2: Nomor Tujuan
    printf("%s\n> ", PROMPT_NOMOR);
    if (!fgets(input_buf, sizeof(input_buf), stdin)) return;

    if (!validasi_nomor_tujuan(db, input_buf, pengirim, nomor_tujuan, pesan_error)) {
        printf("%s\n", pesan_error);
        return;
    }

    // Level 3: Nominal
    printf("Masukkan nominal Transfer Pulsa (Rp%d - Rp%d, kelipatan Rp%d):\n> ", MIN_TRANSFER, MAKS_TRANSFER, KELIPATAN);
    if (!fgets(input_buf, sizeof(input_buf), stdin)) return;

    if (!validasi_nominal(db, input_buf, pengirim, &nominal, pesan_error)) {
        printf("%s\n", pesan_error);
        return;
    }

    // Level 4: Konfirmasi
    cetak_konfirmasi(nomor_tujuan, nominal);
    printf("> ");
    if (!fgets(input_buf, sizeof(input_buf), stdin)) return;

    // Hapus whitespace/newline dari input konfirmasi
    char pil = input_buf[0];
    if (pil != '1') {
        printf("Transfer pulsa dibatalkan.\n");
        return;
    }

    // Eksekusi + Notifikasi
    Transaksi trx = eksekusi_transfer(db, pengirim, nomor_tujuan, nominal);
    notifikasi(db, trx);
}

void sesi_ussd(Database *db, const char *pengirim) {
    char input_buf[50];

    printf("Dial %s\n\n", KODE_USSD);
    printf("%s\n> ", MENU_UTAMA);

    if (!fgets(input_buf, sizeof(input_buf), stdin)) return;

    // Ambil opsi pilihan pertama
    int pilihan = atoi(input_buf);

    if (pilihan == 1) {
        alur_transfer_pulsa(db, pengirim);
    } else if (pilihan >= 2 && pilihan <= 6) {
        printf("Menu ini belum diimplementasikan pada simulasi.\n");
    } else {
        printf("Pilihan tidak valid.\n");
    }
}

// ============================================================================
// MAIN PROGRAM
// ============================================================================
int main(void) {
    Database db = { .jumlah_pelanggan = 0, .jumlah_transaksi = 0 };

    // Inisialisasi Data Pelanggan (Simulasi Database)
    tambah_pelanggan(&db, "081234567890", 50000, true);  // Pengirim
    tambah_pelanggan(&db, "082111222333", 3000, true);   // Penerima aktif
    tambah_pelanggan(&db, "085777888999", 10000, false); // Penerima tidak aktif

    printf("=== SIMULASI USSD *858# (pengirim: 081234567890, saldo Rp50,000) ===\n");
    printf("Nomor tujuan uji: 082111222333 (aktif), 085777888999 (tidak aktif)\n\n");

    sesi_ussd(&db, "081234567890");

    Pelanggan *p_kirim = cari_pelanggan(&db, "081234567890");
    Pelanggan *p_terima = cari_pelanggan(&db, "082111222333");

    printf("\nSaldo akhir pengirim : Rp%d\n", p_kirim ? p_kirim->saldo : 0);
    printf("Saldo akhir penerima : Rp%d\n", p_terima ? p_terima->saldo : 0);

    return 0;
}
