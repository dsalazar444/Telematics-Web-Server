# Telematics Web Server

Un servidor web avanzado escrito en C con funcionalidades de proxy, balance de carga, caché distribuido y replicación. Diseñado para aplicaciones de telemática con requisitos de alta disponibilidad y rendimiento.

## Características Principales

- **Servidor Web**: Servidor HTTP completo con soporte para archivos estáticos
- **Proxy Inverso**: Enrutamiento inteligente de solicitudes a servidores backend
- **Balance de Carga**: Distribución automática de tráfico entre múltiples servidores
- **Health Check**: Verificación periódica de la disponibilidad de servidores backend
- **Caché Distribuido**: Sistema de caché en memoria con limpieza automática
- **Replicación**: Sincronización de datos entre múltiples instancias
- **Logging**: Sistema completo de logging con diferentes niveles
- **Soporte IPv6**: Stack dual (IPv4 e IPv6)
- **Multithreading**: Arquitectura basada en hilos para manejo concurrente

## Estructura del Proyecto

```
├── Config/                 # Configuración del sistema
│   ├── Config.c/h         # Carga de configuración
│   └── proxy.config       # Archivo de configuración del proxy
├── Includes/              # Headers compartidos
│   ├── HttpUtils.c/h      # Utilidades HTTP
│   ├── http.h             # Definiciones HTTP
│   └── ISocket.h          # Interfaz de sockets
├── modules/
│   ├── CacheManager/      # Gestión de caché
│   │   ├── CacheManager.c/h
│   │   ├── CacheIndex.c/h
│   │   └── CacheUtils.c/h
│   ├── HttpParser/        # Análisis de solicitudes HTTP
│   │   ├── HttpParser.c/h
│   │   ├── HttpResponseParser.c/h
│   │   └── UriParser.c/h
│   ├── HttpServer/        # Servidor HTTP
│   │   ├── src/           # Código fuente
│   │   └── www/           # Archivos estáticos (HTML, CSS, etc.)
│   ├── LoadBalancer/      # Balance de carga
│   │   ├── loadBalancer.c/h
│   │   └── HealthCheck.c/h
│   ├── Logs/              # Sistema de logging
│   ├── Socket/            # Manejo de sockets
│   ├── Replicator/        # Replicación de datos
│   └── Worker/            # Trabajadores
│       ├── ProxyWorker.c/h
│       └── ReplicatorWorker.c/h
├── CMakeLists.txt         # Configuración de CMake
└── main.c                 # Punto de entrada del proxy
```

## Compilación

### Requisitos Previos

- CMake >= 3.10
- Compilador C (gcc o clang)
- pthreads (POSIX threads)

### Pasos de Compilación

```bash
# Crear directorio de construcción
mkdir -p build
cd build

# Ejecutar CMake
cmake ..

# Compilar
make

# Los binarios se generarán en el directorio bin/
```

## Ejecución

El proyecto genera dos ejecutables:

### 1. **server** - Servidor Web
```bash
./bin/server
```
Inicia el servidor web principal que sirve archivos estáticos y procesa solicitudes HTTP.

### 2. **pibl** - Proxy/Servidor Proxy
```bash
./bin/pibl
```
Inicia el servidor proxy inverso con:
- Balance de carga
- Caché distribuido
- Health check
- Replicación

Lee la configuración desde `Config/proxy.config`.

## Configuración

El archivo `Config/proxy.config` contiene la configuración del sistema:

```
port=8080                    # Puerto de escucha
logFileProxy=./logs/proxy.log   # Archivo de log del proxy
# ... otros parámetros de configuración
```

## Componentes Principales

### CacheManager
- Gestión de caché en memoria
- Índice de caché para búsquedas rápidas
- Trabajador de limpieza automática
- Utilitarios de serialización

### HttpServer
- Servidor HTTP multithreaded
- Manejador de archivos estáticos
- Generador de respuestas
- Envío eficiente de respuestas HTTP

### LoadBalancer
- Algoritmo de balance de carga
- Health check periódico de servidores backend
- Detección de servidores caídos

### Worker
- **ProxyWorker**: Procesa solicitudes proxy
- **ReplicatorWorker**: Sincroniza datos entre instancias

### Logs
- Sistema de logging con niveles
- Almacenamiento en archivos

## Ejemplos de Uso

### Compilar y ejecutar el servidor
```bash
cd build
cmake ..
make
./bin/server
```

### Compilar y ejecutar el proxy
```bash
cd build
cmake ..
make
./bin/pibl
```

## API HTTP

El servidor proporciona puntos de acceso HTTP estándar:

- `GET /` - Página principal
- `GET /health` - Estado de salud del servidor
- `GET /about` - Información del servidor
- `GET /static/*` - Archivos estáticos

## Logging

El sistema de logging registra eventos importantes en:
- `logs/proxy.log` - Log del proxy
- Consola (stderr/stdout)

## Licencia

Especificar la licencia del proyecto (si aplica)

## Contacto

Para más información sobre el proyecto, consulta la estructura del código y la documentación interna en los headers.

---

**Nota**: Este es un proyecto de telemática orientado a aplicaciones de alta disponibilidad. Asegúrate de revisar la configuración de seguridad antes de desplegar en producción.
