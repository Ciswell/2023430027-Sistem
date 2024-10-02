#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdlib>  

// búfer Circular compartido (implementación como FIFO, pero falta hacerlo circular)
class Buffer {
private:
    std::queue<std::string> buffer;  // (FIFO, no circular)
    int capacity;
    std::mutex mtx;
    std::condition_variable not_full;   // Controla si el búfer está lleno
    std::condition_variable not_empty;  // Controla si el búfer está vacío

public:
    Buffer(int cap) : capacity(cap) {}

    // Insertar en el búfer (el productor espera si está lleno)
    void insertar(const std::string& item) {
        std::unique_lock<std::mutex> lock(mtx);
        not_full.wait(lock, [this]() { return buffer.size() < capacity; });  // (el productor se bloquea, lo cual no debería suceder)
        
        buffer.push(item);
        std::cout << "Insertado: " << item << std::endl;
        not_empty.notify_one();  // Notifica a los consumidores
    }

    // Eliminar del búfer (el consumidor espera si está vacío)
    std::string eliminar() {
        std::unique_lock<std::mutex> lock(mtx);
        not_empty.wait(lock, [this]() { return !buffer.empty(); });  // Consumidor espera si está vacío
        
        std::string item = buffer.front();
        buffer.pop();
        std::cout << "Consumido: " << item << std::endl;
        not_full.notify_one();  // Notifica a los productores
        return item;
    }
};

// Función del productor (el productor debería registrar si no puede insertar)
void productor(Buffer& buffer, int id, int num_items) {
    for (int i = 1; i <= num_items; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000));  // Simula trabajo
        std::string item = "Item_" + std::to_string(id) + "_" + std::to_string(i);
        buffer.insertar(item);  // Intentar insertar en el búfer
    }
}

// Función del consumidor
void consumidor(Buffer& buffer, int num_items) {
    for (int i = 1; i <= num_items; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1500));  // Simula trabajo
        buffer.eliminar();  // Consumir del búfer
    }
}

int main() {
    int NP = 2;  // Número de productores (estos parámetros deben pasar por línea de comandos)
    int NC = 2;  // Número de consumidores
    int BC = 5;  // Capacidad del búfer
    int NPP = 10; // Número de ítems que producirá cada productor
    int NCC = 10; // Número de ítems que consumirá cada consumidor

    Buffer buffer(BC);  // Crear el búfer con la capacidad especificada

    // Crear hebras para productores y consumidores
    std::thread prod1(productor, std::ref(buffer), 1, NPP);
    std::thread prod2(productor, std::ref(buffer), 2, NPP);
    std::thread cons1(consumidor, std::ref(buffer), NCC);
    std::thread cons2(consumidor, std::ref(buffer), NCC);

    // Esperar a que terminen las hebras
    prod1.join();
    prod2.join();
    cons1.join();
    cons2.join();

    return 0;
}

// hacer el búfer un arreglo circular
// hacer que los parámetros queden como entrada
// hacer que el búfer este en memoria compratida entre los procesos
// los archivos de log, registrar la creación de productores y consumidores y la inserción e eliminación de ítem
// hacer que los productores no se bolquen si el búfer esta lleno, sino registrar el intento fallido y reintentar más tarde