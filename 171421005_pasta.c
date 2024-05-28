#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <getopt.h>

#define NUM_USTAS 6        // Toplam usta sayısı
#define MAX_MALZEME 100    // Kuyrukta tutulabilecek maksimum malzeme çifti sayısı

// Malzeme çifti yapısı
typedef struct {
    char malzeme1;
    char malzeme2;
} malzeme_t;

// Malzeme kuyruğu yapısı
typedef struct {
    malzeme_t malzemeler[MAX_MALZEME]; // Malzeme çiftleri dizisi
    int front;                        // Kuyruğun ön ucu
    int rear;                         // Kuyruğun arka ucu
    int count;                        // Kuyrukta bulunan eleman sayısı
    pthread_mutex_t mutex;            // Kuyruğa erişimi kontrol eden mutex
    sem_t malzeme_var;                // Kuyrukta malzeme olup olmadığını belirten semafor
} malzeme_kuyrugu_t;

// Kuyruğu başlatan fonksiyon
void kuyruk_init(malzeme_kuyrugu_t *kuyruk) {
    kuyruk->front = 0;
    kuyruk->rear = -1;
    kuyruk->count = 0;
    pthread_mutex_init(&kuyruk->mutex, NULL);
    sem_init(&kuyruk->malzeme_var, 0, 0);
}

// Kuyruğa malzeme ekleyen fonksiyon
void kuyruk_ekle(malzeme_kuyrugu_t *kuyruk, char malzeme1, char malzeme2) {
    pthread_mutex_lock(&kuyruk->mutex);
    if (kuyruk->count < MAX_MALZEME) {
        kuyruk->rear = (kuyruk->rear + 1) % MAX_MALZEME;
        kuyruk->malzemeler[kuyruk->rear].malzeme1 = malzeme1;
        kuyruk->malzemeler[kuyruk->rear].malzeme2 = malzeme2;
        kuyruk->count++;
        sem_post(&kuyruk->malzeme_var);
    }
    pthread_mutex_unlock(&kuyruk->mutex);
}

// Kuyruktan malzeme alan fonksiyon
int kuyruk_al(malzeme_kuyrugu_t *kuyruk, char *malzeme1, char *malzeme2) {
    sem_wait(&kuyruk->malzeme_var);
    pthread_mutex_lock(&kuyruk->mutex);
    if (kuyruk->count > 0) {
        *malzeme1 = kuyruk->malzemeler[kuyruk->front].malzeme1;
        *malzeme2 = kuyruk->malzemeler[kuyruk->front].malzeme2;
        kuyruk->front = (kuyruk->front + 1) % MAX_MALZEME;
        kuyruk->count--;
        pthread_mutex_unlock(&kuyruk->mutex);
        return 1;
    } else {
        pthread_mutex_unlock(&kuyruk->mutex);
        return 0;
    }
}

// Kuyruğu yok eden fonksiyon
void kuyruk_yoket(malzeme_kuyrugu_t *kuyruk) {
    pthread_mutex_destroy(&kuyruk->mutex);
    sem_destroy(&kuyruk->malzeme_var);
}

// Global değişkenler
pthread_t toptanci;                   // Toptancı thread'i
pthread_t ustalar[NUM_USTAS];         // Usta thread'leri
sem_t toptanci_sem;                   // Toptancı semaforu
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Genel mutex

malzeme_kuyrugu_t malzeme_kuyrugu;    // Malzeme kuyruğu

int pasta_sayisi[NUM_USTAS] = {0};    // Her ustanın ürettiği pasta sayısı
int toptanci_bitti = 0;               // Toptancının işi bitip bitmediği durumu

// Usta bilgilerini tutan yapı
typedef struct {
    int id;             // Ustanın kimlik numarası
    char missing1;      // Ustanın eksik malzemesi 1
    char missing2;      // Ustanın eksik malzemesi 2
    int has_missing1;   // Ustanın eksik malzemesi 1'i alıp almadığı durumu
    int has_missing2;   // Ustanın eksik malzemesi 2'yi alıp almadığı durumu
} usta_t;

// Toptancı thread'ine argüman olarak verilen yapı
typedef struct {
    usta_t *ustalar;    // Ustaların bilgileri
    const char *filePath; // Malzeme dosyasının yolu
} toptanci_arg_t;

// Malzeme adlarını döndüren fonksiyon
const char *get_ingredient_name(char ingredient) {
    switch (ingredient) {
        case 'Y': return "Yumurta";
        case 'U': return "Un";
        case 'S': return "Süt";
        case 'K': return "Kakao";
        default: return "Bilinmeyen";
    }
}

// Ustaları başlatan fonksiyon
void init_ustalar(usta_t ustalar[]) {
    ustalar[0].id = 0; ustalar[0].missing1 = 'S'; ustalar[0].missing2 = 'K'; ustalar[0].has_missing1 = 0; ustalar[0].has_missing2 = 0;
    ustalar[1].id = 1; ustalar[1].missing1 = 'U'; ustalar[1].missing2 = 'S'; ustalar[1].has_missing1 = 0; ustalar[1].has_missing2 = 0;
    ustalar[2].id = 2; ustalar[2].missing1 = 'K'; ustalar[2].missing2 = 'U'; ustalar[2].has_missing1 = 0; ustalar[2].has_missing2 = 0;
    ustalar[3].id = 3; ustalar[3].missing1 = 'Y'; ustalar[3].missing2 = 'K'; ustalar[3].has_missing1 = 0; ustalar[3].has_missing2 = 0;
    ustalar[4].id = 4; ustalar[4].missing1 = 'Y'; ustalar[4].missing2 = 'S'; ustalar[4].has_missing1 = 0; ustalar[4].has_missing2 = 0;
    ustalar[5].id = 5; ustalar[5].missing1 = 'Y'; ustalar[5].missing2 = 'U'; ustalar[5].has_missing1 = 0; ustalar[5].has_missing2 = 0;
}

// Toptancı thread fonksiyonu
void *toptanci_thread(void *arg) {
    toptanci_arg_t *toptanci_arg = (toptanci_arg_t *)arg;
    const char *filePath = toptanci_arg->filePath;

    FILE *file = fopen(filePath, "r");
    if (!file) {
        fprintf(stderr, "Hata: Dosya açılamadı (%s)\n", filePath);
        exit(EXIT_FAILURE);
    }

    char line[3];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n') continue;  // Boş satırları atla
        printf("\n<Toptancı> Teslim edilen malzemeler: %s ve %s\n", get_ingredient_name(line[0]), get_ingredient_name(line[1]));

        kuyruk_ekle(&malzeme_kuyrugu, line[0], line[1]);

        printf("<Toptancı> Pasta yapımını bekliyor...\n");
        sem_post(&toptanci_sem);
        sleep(3);
    }

    fclose(file);

    // Toptancı işini bitirdi
    toptanci_bitti = 1;

    // Ustalara artık malzeme verilmeyeceğini bildir
    for (int i = 0; i < NUM_USTAS; i++) {
        sem_post(&toptanci_sem);
    }

    return NULL;
}

// Usta thread fonksiyonu
void *usta_thread(void *arg) {
    usta_t *usta = (usta_t *)arg;

    while (1) {
        printf("\t[Usta %d] Beklenen malzemeler: %s ve %s\n", usta->id + 1, get_ingredient_name(usta->missing1), get_ingredient_name(usta->missing2));

        sem_wait(&toptanci_sem);

        // Toptancı bitti ve kuyrukta malzeme yoksa döngüden çık
        if (toptanci_bitti && malzeme_kuyrugu.count == 0) {
            break;
        }

        // Kuyruktan malzeme al
        char malzeme1, malzeme2;
        if (!kuyruk_al(&malzeme_kuyrugu, &malzeme1, &malzeme2)) {
            continue;
        }

        pthread_mutex_lock(&mutex);
        int aldi = 0;
        if (usta->missing1 == malzeme1 || usta->missing1 == malzeme2) {
            usta->has_missing1 = 1;
            printf("\t[Usta %d] -> %s aldı.\n", usta->id + 1, get_ingredient_name(usta->missing1));
            aldi = 1;
        } 

        if (usta->missing2 == malzeme1 || usta->missing2 == malzeme2) {
            usta->has_missing2 = 1;
            printf("\t[Usta %d] -> %s aldı.\n", usta->id + 1, get_ingredient_name(usta->missing2));
            aldi = 1;
        }
        pthread_mutex_unlock(&mutex);

        if (!aldi) {
            // Usta malzemeyi almadıysa, tekrar kuyruğa ekleyin
            kuyruk_ekle(&malzeme_kuyrugu, malzeme1, malzeme2);
        }

        // Eğer her iki malzeme de tamamlandıysa pastayı hazırla
        if (usta->has_missing1 && usta->has_missing2) {
            printf("\t[Usta %d] Pasta hazırlanıyor...\n", usta->id + 1);
            sleep(rand() % 5 + 1); // Pasta hazırlık süresini simüle etmek için rastgele uyuma
            printf("\n*****\t[Usta %d] Pasta hazır ve toptancıya teslim edildi.\t*****\n", usta->id + 1);

            // Malzemeleri sıfırla
            usta->has_missing1 = 0;
            usta->has_missing2 = 0;
            pasta_sayisi[usta->id]++;
        }

        // Kuyruk boş ve toptancı malzeme teslim etmeyi bitirdiyse döngüden çık
        if (malzeme_kuyrugu.count == 0 && toptanci_bitti) {
            break;
        }
    }

    return NULL;
}

// Giriş mesajını yazdıran fonksiyon
void print_giris_tasarimi() {
    printf("****************************************************\n");
    printf("*******        Pasta Üretim Sistemi          *******\n");
    printf("****************************************************\n\n");
}

// Çıkış mesajını yazdıran fonksiyon
void print_cikis_tasarimi(int toplam_pasta_sayisi) {
    printf("\n****************************************************\n");
    printf("*******        Üretim Tamamlandı             *******\n");
    printf("*******        Toplam Üretilen Pasta: %d     *******\n", toplam_pasta_sayisi);
    printf("****************************************************\n");
}

int main(int argc, char *argv[]) {
    // Giriş tasarımını yazdır
    print_giris_tasarimi();

    int opt;            // Komut satırı argümanlarını ayrıştırmak için değişken
    char *filePath = NULL;  // Malzeme dosyasının yolu

    // Komut satırı argümanlarını ayrıştır
    while ((opt = getopt(argc, argv, "i:")) != -1) {
        switch (opt) {
            case 'i':
                filePath = optarg;  // Dosya yolunu al
                break;
            default:
                fprintf(stderr, "Kullanım: %s -i <dosya_yolu>\n", argv[0]);
                exit(EXIT_FAILURE); // Hatalı argüman, hata mesajı yazdır ve çık
        }
    }

    // Dosya yolu belirtilmemişse hata ver
    if (filePath == NULL) {
        fprintf(stderr, "Kullanım: %s -i <dosya_yolu>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Toptancı semaforunu başlat
    if (sem_init(&toptanci_sem, 0, 0) != 0) {
        perror("Hata: Toptancı semaforu başlatılamadı");
        exit(EXIT_FAILURE);
    }

    // Usta verilerini başlat
    usta_t ustalar_data[NUM_USTAS];
    init_ustalar(ustalar_data);

    // Malzeme kuyruğunu başlat
    kuyruk_init(&malzeme_kuyrugu);

    // Toptancı argümanlarını oluştur
    toptanci_arg_t toptanci_arg = {ustalar_data, filePath};

    // Toptancı thread'ini oluştur
    if (pthread_create(&toptanci, NULL, toptanci_thread, (void *)&toptanci_arg) != 0) {
        perror("Hata: Toptancı thread oluşturulamadı");
        exit(EXIT_FAILURE);
    }

    // Usta thread'lerini oluştur
    for (int i = 0; i < NUM_USTAS; i++) {
        if (pthread_create(&ustalar[i], NULL, usta_thread, (void *)&ustalar_data[i]) != 0) {
            perror("Hata: Usta thread oluşturulamadı");
            exit(EXIT_FAILURE);
        }
    }

    // Toptancı thread'inin bitmesini bekle
    pthread_join(toptanci, NULL);

    // Usta thread'lerinin bitmesini bekle
    for (int i = 0; i < NUM_USTAS; i++) {
        pthread_join(ustalar[i], NULL);
    }

    printf("\n");

    // Toplam üretilen pasta sayısını hesapla ve yazdır
    int toplam_pasta_sayisi = 0;
    for (int i = 0; i < NUM_USTAS; i++) {
        toplam_pasta_sayisi += pasta_sayisi[i];
        printf("[Usta %d] Toplam üretilen pasta:\t%d\n", ustalar_data[i].id + 1, pasta_sayisi[i]);
    }

    // Çıkış tasarımını yazdır
    print_cikis_tasarimi(toplam_pasta_sayisi);

    // Kaynakları temizle
    sem_destroy(&toptanci_sem);
    pthread_mutex_destroy(&mutex);
    kuyruk_yoket(&malzeme_kuyrugu);

    return 0;
}

