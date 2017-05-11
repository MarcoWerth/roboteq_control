#ifndef SERIAL_CONTROLLER_H
#define SERIAL_CONTROLLER_H

#include <ros/ros.h>
#include <serial/serial.h>

#include <mutex>
#include <condition_variable>  // std::condition_variable

#include <thread>

using namespace std;

namespace roboteq {

/// Read complete callback - Array of callback
typedef function<void (string data) > callback_data_t;

class serial_controller
{
public:
    /**
     * @brief serial_controller Open the serial controller
     * @param port set the port
     * @param set the baudrate
     */
    serial_controller(string port, unsigned long baudrate);

    ~serial_controller();
    /**
     * @brief start Initialize the serial communcation
     * @return if open the connection return true
     */
    bool start();
    /**
     * @brief stop
     * @return
     */
    bool stop();

    bool command(string msg, string params="");

    bool query(string msg, string params="", string type="?");
    /**
     * @brief get Get the message parsed
     * @return Return the string received
     */
    string get()
    {
        return sub_data;
    }

    /**
     * @brief script Run and stop the script inside the Roboteq
     * @param status The status of the script
     * @return the status of command
     */
    bool script(bool status) {
        if(status)
        {
            return command("R");
        } else
        {
            return command("R", "0");
        }
    }
    /**
     * @brief echo Enable or disable the echo message
     * @param type status of echo
     * @return The status of command
     */
    bool echo(bool type) {
        if(type) {
            return command("ECHOF", "1");
        } else
        {
            return command("ECHOF", "0");
        }
    }
    /**
     * @brief addCallback Add callback message
     * @param callback The callback function
     * @param type The type of message to check
     */
    bool addCallback(const callback_data_t &callback, const string data);
    /**
     * Template to connect a method in callback
     */
    template <class T> bool addCallback(void(T::*fp)(const string), T* obj, const string data) {
        return addCallback(bind(fp, obj, _1), data);
    }

protected:

private:
    // Serial port object
    serial::Serial mSerial;
    // Serial port name
    string mSerialPort;
    // Serial port baudrate
    uint32_t mBaudrate;
    // Timeout open serial port
    uint32_t mTimeout;
    // Used to stop the serial processing
    bool mStopping;
    // Last message sent
    string mMessage;
    string sub_data;
    bool sub_data_cmd;
    bool data;
    // Async reader controller
    std::thread first;
    // Mutex to sto concurent sending
    mutex mWriteMutex;
    mutex mReaderMutex;
    std::condition_variable cv;
    // Hashmap with all type of message
    map<string, callback_data_t> hashmap;

    // Async reader from serial
    void async_reader();
};

}

#endif // SERIAL_CONTROLLER_H
