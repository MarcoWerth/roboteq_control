

#include "roboteq/roboteq.h"

namespace roboteq
{

Roboteq::Roboteq(const ros::NodeHandle &nh, const ros::NodeHandle &private_nh, serial_controller *serial)
    : DiagnosticTask("Roboteq")
    , mNh(nh)
    , private_mNh(private_nh)
    , mSerial(serial)
{
    // First run dynamic reconfigurator
    setup_controller = false;

    _first = false;
    std::vector<std::string> joint_list;
    if(private_nh.hasParam("joint"))
    {
        private_nh.getParam("joint", joint_list);
    }
    else
    {
        _first = true;
        ROS_WARN("No joint list!");
        joint_list.push_back("joint_0");
        joint_list.push_back("joint_1");
        private_nh.setParam("joint", joint_list);
    }

    // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    // TODO MUST TO BE REMOVE
    // ONLY FOR TEST
    _first = true;
    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    // Disable ECHO
    mSerial->echo(false);
    // Disable Script and wait to load all parameters
    mSerial->script(false);
    // Stop motors
    mSerial->command("EX");

    // Initialize Joints
    for(unsigned i=0; i < joint_list.size(); ++i)
    {
        string motor_name = joint_list.at(i);
        int number = i;
        if(private_nh.hasParam(motor_name + "/number"))
        {
            private_nh.getParam(motor_name + "/number", number);
        }
        else
        {
            ROS_WARN_STREAM("Default number selected for Motor: " << motor_name << " is " << number);
            private_nh.setParam(motor_name + "/number", number);
        }

        ROS_INFO_STREAM("Motor[" << number << "] name: " << motor_name);
        mMotor[motor_name] = new Motor(private_mNh, serial, motor_name, number);
        mMotorName[number] = motor_name;
    }

    if(mSerial->query("FID"))
    {
        ROS_INFO_STREAM("Data=" << mSerial->get());
    }

    if(mSerial->query("TRN"))
    {
        ROS_INFO_STREAM("Data=" << mSerial->get());
    }
}

Roboteq::~Roboteq()
{
    // ROS_INFO_STREAM("Script: " << script(false));
}

void Roboteq::initialize()
{
    // Check if is required load paramers
    if(_first)
    {
        // Load parameters from roboteq
        getControllerFromRoboteq();
    }

    // Initialize parameter dynamic reconfigure
    ds_controller = new dynamic_reconfigure::Server<roboteq_control::RoboteqControllerConfig>(private_mNh);
    dynamic_reconfigure::Server<roboteq_control::RoboteqControllerConfig>::CallbackType cb_controller = boost::bind(&Roboteq::reconfigureCBController, this, _1, _2);
    ds_controller->setCallback(cb_controller);

    // Initialize all motors in list
    for( map<string, Motor*>::iterator ii=mMotor.begin(); ii!=mMotor.end(); ++ii)
    {
        // Launch initialization motors
        (*ii).second->initializeMotor(_first);
        ROS_DEBUG_STREAM("Motor [" << (*ii).first << "] Initialized");
    }
    // Enable script
    mSerial->script(true);
}

void Roboteq::initializeInterfaces()
{
    // Initialize the diagnostic from the primitive object
    initializeDiagnostic();

    if (!model.initParam("/robot_description")){
        ROS_ERROR("Failed to parse urdf file");
    }
    else
    {
        ROS_INFO_STREAM("/robot_description found! " << model.name_ << " parsed!");
    }

    for( map<string, Motor*>::iterator ii=mMotor.begin(); ii!=mMotor.end(); ++ii)
    {
        /// State interface
        joint_state_interface.registerHandle(((*ii).second)->joint_state_handle);
        /// Velocity interface
        velocity_joint_interface.registerHandle(((*ii).second)->joint_handle);

        // Setup limits
        ((*ii).second)->setupLimits(model);

        // reset position joint
        double position = 0;
        ROS_DEBUG_STREAM("Motor [" << (*ii).first << "] reset position to: " << position);
        ((*ii).second)->resetPosition(position);

        //Add motor in diagnostic updater
        diagnostic_updater.add(*((*ii).second));
        ROS_DEBUG_STREAM("Motor [" << (*ii).first << "] Registered");
    }
    ROS_DEBUG_STREAM("Send all Constraint configuration");

    /// Register interfaces
    registerInterface(&joint_state_interface);
    registerInterface(&velocity_joint_interface);
}

void Roboteq::initializeDiagnostic() {

}

void Roboteq::read(const ros::Time& time, const ros::Duration& period) {
    //ROS_DEBUG_STREAM("Get measure from Roboteq");
    for( map<string, Motor*>::iterator ii=mMotor.begin(); ii!=mMotor.end(); ++ii)
    {
        // Not required now
        // TODO
        //(*ii).second->addRequestMeasure();
        ROS_DEBUG_STREAM("Motor [" << (*ii).first << "] Request measures");
    }
}

void Roboteq::write(const ros::Time& time, const ros::Duration& period) {
    //ROS_DEBUG_STREAM("Write command to Roboteq");
    for( map<string, Motor*>::iterator ii=mMotor.begin(); ii!=mMotor.end(); ++ii)
    {
        (*ii).second->writeCommandsToHardware(period);
        ROS_DEBUG_STREAM("Motor [" << (*ii).first << "] Send commands");
    }
}

bool Roboteq::prepareSwitch(const std::list<hardware_interface::ControllerInfo>& start_list, const std::list<hardware_interface::ControllerInfo>& stop_list)
{
    ROS_INFO_STREAM("Prepare to switch!");
    return true;
}

void Roboteq::doSwitch(const std::list<hardware_interface::ControllerInfo>& start_list, const std::list<hardware_interface::ControllerInfo>& stop_list)
{
    // Stop all controller in list
    for(std::list<hardware_interface::ControllerInfo>::const_iterator it = stop_list.begin(); it != stop_list.end(); ++it)
    {
        //ROS_INFO_STREAM("DO SWITCH STOP name: " << it->name << " - type: " << it->type);
        const hardware_interface::InterfaceResources& iface_res = it->claimed_resources.front();
        for (std::set<std::string>::const_iterator res_it = iface_res.resources.begin(); res_it != iface_res.resources.end(); ++res_it)
        {
            ROS_INFO_STREAM(it->name << "[" << *res_it << "] STOP");
            mMotor[*res_it]->switchController("disable");
        }
    }
    // Stop script
    mSerial->script(false);
    // Stop motors
    mSerial->command("EX");
    // Run all new controllers
    for(std::list<hardware_interface::ControllerInfo>::const_iterator it = start_list.begin(); it != start_list.end(); ++it)
    {
        //ROS_INFO_STREAM("DO SWITCH START name: " << it->name << " - type: " << it->type);
        const hardware_interface::InterfaceResources& iface_res = it->claimed_resources.front();
        for (std::set<std::string>::const_iterator res_it = iface_res.resources.begin(); res_it != iface_res.resources.end(); ++res_it)
        {
            ROS_INFO_STREAM(it->name << "[" << *res_it << "] START");
            mMotor[*res_it]->switchController(it->type);
        }
    }
    // Run script
    mSerial->script(true);
    // Enable motor
    mSerial->command("MG");
}

void Roboteq::run(diagnostic_updater::DiagnosticStatusWrapper &stat) {

}

void Roboteq::getControllerFromRoboteq()
{
    try
    {
        // Get PWM frequency PWMF [pag. 327]
        string str_pwm = mSerial->getParam("PWMF");
        unsigned int tmp_pwm = boost::lexical_cast<unsigned int>(str_pwm);
        double pwm = ((double) tmp_pwm) / 10.0;
        // Set params
        private_mNh.setParam("pwm_frequency", pwm);

        // Get over voltage limit OVL [pag. 326]
        string str_ovl = mSerial->getParam("OVL");
        unsigned int tmp_ovl = boost::lexical_cast<unsigned int>(str_ovl);
        double ovl = ((double) tmp_ovl) / 10.0;
        // Set params
        private_mNh.setParam("over_voltage_limit", ovl);

        // Get over voltage hystersis OVH [pag. 326]
        string str_ovh = mSerial->getParam("OVH");
        unsigned int tmp_ovh = boost::lexical_cast<unsigned int>(str_ovh);
        double ovh = ((double) tmp_ovh) / 10.0;
        // Set params
        private_mNh.setParam("over_voltage_hysteresis", ovh);

        // Get under voltage limit UVL [pag. 328]
        string str_uvl = mSerial->getParam("UVL");
        unsigned int tmp_uvl = boost::lexical_cast<unsigned int>(str_uvl);
        double uvl = ((double) tmp_uvl) / 10.0;
        // Set params
        private_mNh.setParam("under_voltage_limit", uvl);

        // Get brake activation delay BKD [pag. 309]
        string str_break = mSerial->getParam("BKD");
        int break_delay = boost::lexical_cast<unsigned int>(str_break);
        // Set params
        private_mNh.setParam("break_delay", break_delay);

        // Get Mixing mode MXMD [pag. 322]
        string str_mxd = mSerial->getParam("MXMD", "1");
        ROS_INFO_STREAM("MXMD "<< str_mxd);
        int mixed = boost::lexical_cast<unsigned int>(str_mxd);
        // Set params
        private_mNh.setParam("mixing", mixed);

    } catch (std::bad_cast& e)
    {
        ROS_WARN_STREAM("Failure parsing feedback data. Dropping message." << e.what());
    }
}

void Roboteq::reconfigureCBController(roboteq_control::RoboteqControllerConfig &config, uint32_t level)
{
    //The first time we're called, we just want to make sure we have the
    //original configuration
    if(!setup_controller)
    {
      _last_controller_config = config;
      default_controller_config = _last_controller_config;
      setup_controller = true;
      return;
    }

    if(config.restore_defaults)
    {
        //if someone sets restore defaults on the parameter server, prevent looping
        config.restore_defaults = false;
        // Overload config with default
        config = default_controller_config;
    }

    if(config.load_roboteq)
    {
        //if someone sets again the request on the parameter server, prevent looping
        config.load_roboteq = false;
        // Launch param load
        getControllerFromRoboteq();
        // Skip other read
        return;
    }

    // Set PWM frequency PWMF [pag. 327]
    if(_last_controller_config.pwm_frequency != config.pwm_frequency)
    {
        // Update PWM
        int pwm = config.pwm_frequency * 10;
        mSerial->setParam("PWMF", std::to_string(pwm));
    }
    // Set over voltage limit OVL [pag. 326]
    if(_last_controller_config.over_voltage_limit != config.over_voltage_limit)
    {
        // Update over voltage limit
        int ovl = config.over_voltage_limit * 10;
        mSerial->setParam("OVL", std::to_string(ovl));
    }
    // Set over voltage hystersis OVH [pag. 326]
    if(_last_controller_config.over_voltage_hysteresis != config.over_voltage_hysteresis)
    {
        // Update over voltage hysteresis
        int ovh = config.over_voltage_hysteresis * 10;
        mSerial->setParam("OVH", std::to_string(ovh));
    }
    // Set under voltage limit UVL [pag. 328]
    if(_last_controller_config.under_voltage_limit != config.under_voltage_limit)
    {
        // Update under voltage limit
        int uvl = config.under_voltage_limit * 10;
        mSerial->setParam("UVL", std::to_string(uvl));
    }
    // Set brake activation delay BKD [pag. 309]
    if(_last_controller_config.break_delay != config.break_delay)
    {
        // Update brake activation delay
        mSerial->setParam("BKD", std::to_string(config.break_delay));
    }

    // Set Mixing mode MXMD [pag. 322]
    if(_last_controller_config.mixing != config.mixing)
    {
        // Update brake activation delay
        mSerial->setParam("MXMD", std::to_string(config.mixing) + ":0");
    }

    // Update last configuration
    _last_controller_config = config;
}

}
