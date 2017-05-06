#ifndef ROBOTEQ_H
#define ROBOTEQ_H

#include <ros/ros.h>
#include <serial/serial.h>

#include <diagnostic_updater/diagnostic_updater.h>
#include <diagnostic_updater/publisher.h>
#include <hardware_interface/robot_hw.h>

#include <roboteq_control/RoboteqControllerConfig.h>

#include "roboteq/serial_controller.h"
#include "roboteq/motor.h"

using namespace std;

namespace roboteq
{

typedef struct joint
{
    Motor *motor;
    // State of the motor
    double position;
    double velocity;
    double effort;
    double velocity_command;
} joint_t;

typedef struct _status_flag {
    uint8_t serial_mode : 1;
    uint8_t pulse_mode : 1;
    uint8_t analog_mode : 1;
    uint8_t spectrum : 1;
    uint8_t power_stage_off : 1;
    uint8_t stall_detect : 1;
    uint8_t at_limit : 1;
    uint8_t microbasic_running : 1;
} status_flag_t;
// Reference in pag 245
typedef struct _status_fault {
    uint8_t overheat : 1;
    uint8_t overvoltage : 1;
    uint8_t undervoltage : 1;
    uint8_t short_circuit : 1;
    uint8_t emergency_stop : 1;
    uint8_t brushless_sensor_fault : 1;
    uint8_t mosfet_failure : 1;
} status_fault_t;

class Roboteq : public hardware_interface::RobotHW, public diagnostic_updater::DiagnosticTask
{
public:
    /**
     * @brief serial_controller Open the serial controller
     * @param port set the port
     * @param set the baudrate
     */
    Roboteq(const ros::NodeHandle &nh, const ros::NodeHandle &private_nh, serial_controller *serial);

    ~Roboteq();

    void run(diagnostic_updater::DiagnosticStatusWrapper &stat);

    /**
     * @brief initialize
     */
    void initialize();

    /**
     * @brief initializeInterfaces Initialize all motors.
     * Add all Control Interface availbles and add in diagnostic task
     */
    void initializeInterfaces();
    /**
     * @brief updateDiagnostics
     */
    void updateDiagnostics();

    void initializeDiagnostic();

    void write(const ros::Time& time, const ros::Duration& period);

    void read(const ros::Time& time, const ros::Duration& period);

    bool prepareSwitch(const std::list<hardware_interface::ControllerInfo>& start_list, const std::list<hardware_interface::ControllerInfo>& stop_list);

    void doSwitch(const std::list<hardware_interface::ControllerInfo>& start_list, const std::list<hardware_interface::ControllerInfo>& stop_list);

private:
    //Initialization object
    //NameSpace for bridge controller
    ros::NodeHandle mNh;
    ros::NodeHandle private_mNh;
    // Serial controller
    serial_controller *mSerial;
    // Diagnostic
    diagnostic_updater::Updater diagnostic_updater;

    /// URDF information about robot
    urdf::Model model;

    /// ROS Control interfaces
    hardware_interface::JointStateInterface joint_state_interface;
    hardware_interface::VelocityJointInterface velocity_joint_interface;

    // Check if is the first run
    bool _first;
    // Motor definition
    map<string, Motor*> mMotor;
    //map<int, string> mMotorName;

    string _type, _model;
    string _version;
    string _uid;
    string _script_ver;

    status_flag_t _flag;
    status_fault_t _fault;
    double _volts_internal, _volts_five;
    double _temp_mcu, _temp_bridge;

    /**
     * @brief getRoboteqInformation Load basic information from roboteq board
     */
    void getRoboteqInformation();

    /// Setup variable
    bool setup_controller;

    /// Dynamic reconfigure PID
    dynamic_reconfigure::Server<roboteq_control::RoboteqControllerConfig> *ds_controller;
    /**
     * @brief reconfigureCBEncoder when the dynamic reconfigurator change some values start this method
     * @param config variable with all configuration from dynamic reconfigurator
     * @param level
     */
    void reconfigureCBController(roboteq_control::RoboteqControllerConfig &config, uint32_t level);

    // Default parameter config
    roboteq_control::RoboteqControllerConfig default_controller_config, _last_controller_config;


    /**
     * @brief status The status associated of roboteq board
     * @param data The data to decode
     */
    void status(string data);
    /**
     * @brief getPIDFromRoboteq Load PID parameters from Roboteq board
     */
    void getControllerFromRoboteq();

};

}


#endif // ROBOTEQ_H
