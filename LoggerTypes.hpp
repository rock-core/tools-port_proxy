#ifndef LOGGER_TYPES_HPP
#define LOGGER_TYPES_HPP

#include <base/time.h>

namespace logger {
  enum MarkerType{
      SingleEvent=0,
      Stop,
      Start
  };

  struct Marker {
    int id;
    logger::MarkerType type;
    std::string comment;
    base::Time time;
  };
};


#endif
