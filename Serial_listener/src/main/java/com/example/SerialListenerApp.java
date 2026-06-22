package com.example;

import com.fazecast.jSerialComm.SerialPort;
import com.fazecast.jSerialComm.SerialPortDataListener;
import com.fazecast.jSerialComm.SerialPortEvent;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.*;
import java.util.concurrent.atomic.AtomicBoolean;

public class SerialListenerApp {

    private static final DateTimeFormatter FILE_TS = DateTimeFormatter.ofPattern("yyyyMMdd_HHmmss");
    private static final DateTimeFormatter LOG_TS = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");

    private static final String DATA_PATH = "./data";
    private static final int FLUSH_THRESHOLD = 100;

    private static final AtomicBoolean running = new AtomicBoolean(true);
    private static final List<String> buffer = new ArrayList<>();
    private static final StringBuilder serialBuffer = new StringBuilder();

    private static volatile SerialPort port;
    private static String portName;
    private static Path currentFile;

    private static synchronized void createNewFile() throws IOException {
        currentFile = Paths.get(DATA_PATH, LocalDateTime.now().format(FILE_TS) + "_" + portName + ".csv");
        Files.write(
                currentFile, Collections.singletonList("timestamp,volts,amps,watt,watthour"),
                StandardOpenOption.CREATE, StandardOpenOption.TRUNCATE_EXISTING);

        buffer.clear();
        System.out.println();
        System.out.println("=================================================");
        System.out.println("Logging data to: " + currentFile.toAbsolutePath());
        System.out.println("=================================================");
        System.out.println();
    }

    private static synchronized void flushBuffer() {
        if (buffer.isEmpty()) { return; }
        try {
            Files.write(currentFile, buffer, StandardOpenOption.APPEND);
            System.out.printf("[%s] Saved %d records%n", LocalDateTime.now().format(LOG_TS), buffer.size());
            buffer.clear();
        } catch (IOException ex) {
            ex.printStackTrace();
        }
    }

    private static synchronized void addRecord(String record) {
        buffer.add(record);
        if (buffer.size() >= FLUSH_THRESHOLD) {
            flushBuffer();
        }
    }

    private static synchronized void rotateFile() {
        try {
            flushBuffer();
            createNewFile();
        } catch (IOException ex) {
            ex.printStackTrace();
        }
    }

    private static synchronized void processBytes(byte[] bytes) {
        serialBuffer.append(new String(bytes, StandardCharsets.UTF_8));
        int newline;
        while ((newline = serialBuffer.indexOf("\n")) >= 0) {
            String line = serialBuffer.substring(0, newline);
            serialBuffer.delete(0, newline + 1);
            handleLine(line.trim());
        }
    }

    private static void handleLine(String line) {
        if (line.isBlank()) { return; }
        if ("START".equalsIgnoreCase(line)) {
            System.out.println("START signal received.");
            if (!buffer.isEmpty()) { rotateFile(); }
            return;
        }

        String[] parts = line.split(",");
        if (parts.length != 4) {
            System.out.println("Invalid line ignored: " + line);
            return;
        }

        String timestamp = LocalDateTime.now().format(LOG_TS);
        addRecord(timestamp + "," + parts[0] + "," + parts[1] + "," + parts[2] + "," + parts[3]);
        System.out.printf("%s: %8sV, %8sA, %8sW, %8sWh%n", timestamp, parts[0], parts[1], parts[2], parts[3]);
    }

    private static void registerListener(SerialPort serialPort) {

        serialPort.addDataListener(new SerialPortDataListener() {
            @Override
            public int getListeningEvents() {
                return SerialPort.LISTENING_EVENT_DATA_RECEIVED | SerialPort.LISTENING_EVENT_PORT_DISCONNECTED;
            }

            @Override
            public void serialEvent(SerialPortEvent event) {
                if (event.getEventType() == SerialPort.LISTENING_EVENT_PORT_DISCONNECTED) {
                    System.out.println("[" + LocalDateTime.now().format(LOG_TS) + "] Device disconnected.");
                    try {
                        event.getSerialPort().removeDataListener();
                        event.getSerialPort().closePort();
                    }
                    catch (Exception ignored) {}
                    port = null;
                    return;
                }

                if (event.getEventType() == SerialPort.LISTENING_EVENT_DATA_RECEIVED) {
                    processBytes(event.getReceivedData());
                }
            }
        });
    }

    private static void startConnectionSupervisor() {
        Thread supervisor = new Thread(() -> {
            while (running.get()) {
                try {
                    if (port != null && port.isOpen() && port.getInputStream() != null) { 
                        Thread.sleep(1000); continue; 
                    }

                    System.out.printf("[%s] Waiting for %s...%n", LocalDateTime.now().format(LOG_TS), portName);
                    SerialPort candidate = SerialPort.getCommPort(portName);
                    
                    candidate.setComPortParameters(115200, 8, SerialPort.ONE_STOP_BIT, SerialPort.NO_PARITY);
                    candidate.setComPortTimeouts(SerialPort.TIMEOUT_NONBLOCKING, 0, 0);
                    
                    if (!candidate.openPort()) { 
                        Thread.sleep(1000); continue; 
                    }

                    registerListener(candidate); 
                    port = candidate;
                    System.out.printf("[%s] Connected to %s%n", LocalDateTime.now().format(LOG_TS), portName);
                } catch (Exception ex) {
                    ex.printStackTrace();
                    try { Thread.sleep(1000); } 
                    catch (InterruptedException ignored) {}
                }
            }
        }, "connection-supervisor");
        supervisor.setDaemon(true);
        supervisor.start();
    }

    private static void startFlushThread() {
        Thread flushThread = new Thread(() -> {
            while (running.get()) {
                try {
                    Thread.sleep(10000);
                    flushBuffer();
                } 
                catch (Exception ignored) {}
            }
        }, "flush-thread");
        flushThread.setDaemon(true);
        flushThread.start();
    }

    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            System.out.println("Usage example:");
            System.out.println("  \"SerialLogger/SerialLogger.exe\" COM5");
            return;
        }

        portName = args[0];
        Files.createDirectories(Paths.get(DATA_PATH));
        System.out.println("Welcome to serial logging utility...");

        createNewFile();
        startFlushThread();
        startConnectionSupervisor();

        Runtime.getRuntime().addShutdownHook(new Thread(() -> {
            running.set(false);
            System.out.println("Shutting down...");
            flushBuffer();
            try {
                if (port != null && port.isOpen()) {
                    port.removeDataListener();
                    port.closePort();
                }
            }
            catch (Exception ignored) {}
        }));

        while (running.get()) {
            Thread.sleep(1000);
        }
    }
}
