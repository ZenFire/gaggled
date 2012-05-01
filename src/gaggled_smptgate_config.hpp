#ifndef GAGGLED_SMTPGATE_CONFIG_HPP
#define GAGGLED_SMTPGATE_CONFIG_HPP

  std::string controlurl;
  std::string eventurl;
  int period;
  int batch;
  std::string prefix;
  std::string mx;
  std::string helo;
  std::string from;
  std::string to;

  boost::property_tree::ptree config;
  std::string which_setting = "";

  try {
    boost::property_tree::read_info(conf_file, config);
  } catch (boost::property_tree::info_parser::info_parser_error& pe) {
    SMTP_CONFIGTEST_FAIL("gaggled_smtpgate: configtest: parse error at " + pe.filename() + ":" + boost::lexical_cast<std::string>(pe.line()) + ": " + pe.message());
  }

  try {
    which_setting = "gaggled.controlurl";
    controlurl = config.get<std::string>(which_setting);
    which_setting = "gaggled.eventurl";
    eventurl = config.get<std::string>(which_setting);

    which_setting = "gaggled.smtpgate.mx";
    mx = config.get<std::string>(which_setting);
    which_setting = "gaggled.smtpgate.helo";
    helo = config.get<std::string>(which_setting);
    which_setting = "gaggled.smtpgate.from";
    from = config.get<std::string>(which_setting);
    which_setting = "gaggled.smtpgate.to";
    to = config.get<std::string>(which_setting);

    period = config.get<int>("gaggled.smtpgate.period", 10000);
    if (period < 1) {
      SMTP_CONFIGTEST_FAIL("gaggled_smtpgate: configtest: gaggled.smtpgate.period must be a positive integer");
    }
    batch = config.get<int>("gaggled.smtpgate.batch", 0);
    if (batch < 0) {
      SMTP_CONFIGTEST_FAIL("gaggled_smtpgate: configtest: gaggled.smtpgate.batch must be a non-negative integer");
    }

    prefix = config.get<std::string>("gaggled.smtpgate.spfix", "");
  } catch (boost::property_tree::ptree_bad_path& pbp) {
    SMTP_CONFIGTEST_FAIL("gaggled_smtpgate: configtest: required setting " + which_setting + " is not set in " + std::string(conf_file) + ".");
  }

#endif
