#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <csignal>
#include <atomic>

// Señal global para controlar interrupciones
std::atomic<bool> signal_received(false);

void signalHandler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\nSeñal SIGINT recibida. Terminando el programa de forma ordenada..." << std::endl;
        signal_received = true;
    }
}

class Buffer {
private:
    std::vector<std::string> buffer;
    int capacity;
    int front, rear;
    int buffer_size;
    std::mutex mtx;
    std::condition_variable not_full, not_empty;
    bool finished;

public:
    Buffer(int cap) : capacity(cap), front(0), rear(0), buffer_size(0), finished(false) {
        buffer.resize(capacity);  
    }

    void insertar(const std::string& item, std::ofstream& log, int productor_id, std::mutex& log_mtx) {
        std::unique_lock<std::mutex> lock(mtx);

        not_full.wait(lock, [this]() { return buffer_size < capacity || finished; });

        if (item.empty()) {
            std::lock_guard<std::mutex> log_lock(log_mtx);
            log << "Error: Productor " << productor_id << " intentó insertar un ítem vacío." << std::endl;
            return;
        }

        buffer[rear] = item;
        rear = (rear + 1) % capacity;
        buffer_size++;

        {
            std::lock_guard<std::mutex> log_lock(log_mtx);
            log << "Productor " << productor_id << " insertó: " << item << std::endl;
        }

        not_empty.notify_one();  
    }

    std::string eliminar(std::ofstream& log, int consumidor_id, std::mutex& log_mtx) {
        std::unique_lock<std::mutex> lock(mtx);

        not_empty.wait(lock, [this]() { return buffer_size > 0 || finished; });

        if (buffer_size == 0 && finished) return "";  // Si se terminó el consumo, devolver vacío

        // Eliminar ítem del búfer
        std::string item = buffer[front];
        front = (front + 1) % capacity;
        buffer_size--;

        // Log de la eliminación
        {
            std::lock_guard<std::mutex> log_lock(log_mtx);
            log << "Consumidor " << consumidor_id << " consumió: " << item << std::endl;
        }

        not_full.notify_one();  // Notificar a un productor
        return item;
    }

    void setFinished() {
        std::unique_lock<std::mutex> lock(mtx);
        finished = true;
        not_empty.notify_all();  // Notificar a todos los consumidores
    }
};

class Productor {
private:
    Buffer& buffer;
    int id;
    int num_items;

public:
    Productor(Buffer& buf, int id, int num_items) : buffer(buf), id(id), num_items(num_items) {}

    void producir(std::ofstream& log, std::mutex& log_mtx) {
        {
            std::lock_guard<std::mutex> log_lock(log_mtx);
            log << "Productor " << id << " creado." << std::endl;
        }

        for (int i = 1; i <= num_items; ++i) {
            if (signal_received) break;  // Detenerse si se recibe la señal SIGINT
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000));

            // Generar el ítem
            std::string item = "Item_" + std::to_string(id) + "_" + std::to_string(i);

            // Verificar que el ítem está correctamente formado
            if (item.empty()) {
                std::lock_guard<std::mutex> log_lock(log_mtx);
                log << "Error: Productor " << id << " generó un ítem vacío." << std::endl;
                continue;
            }

            buffer.insertar(item, log, id, log_mtx);  // Insertar en el búfer
        }

        {
            std::lock_guard<std::mutex> log_lock(log_mtx);
            log << "Productor " << id << " terminó." << std::endl;
        }
    }
};

class Consumidor {
private:
    Buffer& buffer;
    int id;
    int num_items;

public:
    Consumidor(Buffer& buf, int id, int num_items) : buffer(buf), id(id), num_items(num_items) {}

    void consumir(std::ofstream& log, std::mutex& log_mtx) {
        {
            std::lock_guard<std::mutex> log_lock(log_mtx);
            log << "Consumidor " << id << " creado." << std::endl;
        }

        for (int i = 1; i <= num_items; ++i) {
            if (signal_received) break;  // Detenerse si se recibe la señal SIGINT
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1500));

            // Consumir del búfer
            std::string item = buffer.eliminar(log, id, log_mtx);

            // Verificar si el ítem está vacío
            if (item.empty()) {
                std::lock_guard<std::mutex> log_lock(log_mtx);
                log << "Error: Consumidor " << id << " recibió un ítem vacío." << std::endl;
                break;
            }
        }

        {
            std::lock_guard<std::mutex> log_lock(log_mtx);
            log << "Consumidor " << id << " terminó." << std::endl;
        }
    }
};

class Principal {
private:
    int NP, NC, BC, NPP, NCC;

public:
    Principal(int np, int nc, int bc, int npp, int ncc) : NP(np), NC(nc), BC(bc), NPP(npp), NCC(ncc) {}

    void iniciar() {
        Buffer buffer(BC);
        std::ofstream log_productores("log_productores.txt");
        std::ofstream log_consumidores("log_consumidores.txt");
        std::mutex log_mtx;

        if (!log_productores.is_open() || !log_consumidores.is_open()) {
            std::cerr << "Error al abrir los archivos de log." << std::endl;
            return;
        }

        std::vector<std::thread> productores, consumidores;

        // Crear hilos de productores
        for (int i = 1; i <= NP; ++i) {
            productores.emplace_back([=, &buffer, &log_productores, &log_mtx]() mutable {
                Productor productor(buffer, i, NPP);
                productor.producir(log_productores, log_mtx);
            });
        }

        // Crear hilos de consumidores
        for (int i = 1; i <= NC; ++i) {
            consumidores.emplace_back([this, i, &buffer, &log_consumidores, &log_mtx]() {
                Consumidor consumidor(buffer, i, NCC);
                consumidor.consumir(log_consumidores, log_mtx);
            });
        }

        // Esperar a que terminen los productores
        for (auto& t : productores) t.join();
        buffer.setFinished();  // Señalar que los productores terminaron

        // Esperar a que terminen los consumidores
        for (auto& t : consumidores) t.join();

        log_productores.close();
        log_consumidores.close();
    }
};

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Error: Se requieren 5 parámetros." << std::endl;
        return 1;
    }

    signal(SIGINT, signalHandler);

    int NP = std::stoi(argv[1]);
    int NC = std::stoi(argv[2]);
    int BC = std::stoi(argv[3]);
    int NPP = std::stoi(argv[4]);
    int NCC = std::stoi(argv[5]);

    Principal p(NP, NC, BC, NPP, NCC);
    p.iniciar();

    return 0;
}
