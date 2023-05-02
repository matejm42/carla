#define _GLIBCXX_USE_CXX11_ABI 0

#include "ImageListener.h"
#include <iostream>

#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/core/status/PublicationMatchedStatus.hpp>

namespace carla {
namespace ros2 {

  namespace efd = eprosima::fastdds::dds;

    class CarlaImageListenerImpl : public efd::DataWriterListener {
      public:
      void on_publication_matched(
              efd::DataWriter* writer,
              const efd::PublicationMatchedStatus& info) override;
                  

      int _matched {0};
      bool _first_connected {false};
    };

    void CarlaImageListenerImpl::on_publication_matched(efd::DataWriter* writer, const efd::PublicationMatchedStatus& info)
    {
      if (info.current_count_change == 1) {
          _matched = info.total_count;
          _first_connected = true;
          std::cout << "Publisher matched." << std::endl;
      } else if (info.current_count_change == -1) {
          _matched = info.total_count;
          std::cout << "Publisher unmatched." << std::endl;
      } else {
          std::cerr << info.current_count_change
                  << " is not a valid value for PublicationMatchedStatus current count change" << std::endl;
      }
    }

    CarlaImageListener::CarlaImageListener() :
    _impl(std::make_unique<CarlaImageListenerImpl>()) { }

    CarlaImageListener::~CarlaImageListener() {}

}}
