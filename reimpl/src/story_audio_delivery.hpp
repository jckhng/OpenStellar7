#pragma once
#include "stellar7/native_delivery.hpp"

namespace stellar7 {
//Transport prefixes borrow no owners and leave the complete receipt unchanged.
class StoryDeliveryStream {
public:
    explicit StoryDeliveryStream(const NativeDeliverySink& sink):sink_(sink) {}
    void adopt(const NativeSessionDelivery& delivery) noexcept {
        samples_=delivery.samples.size();presentations_=delivery.presentations.size();events_=delivery.events.size();
    }
    void publish(const NativeSessionDelivery& delivery) {
        auto ready=presentations_;
        const auto end=delivery.firstSample+delivery.samples.size();
        // Composite helpers may construct later frame marks before their PCM
        // prefixes are appended. Hold those marks until their audio is available.
        while(ready<delivery.presentations.size()&&delivery.presentations[ready].sampleEnd<=end) {++ready;}
        if(!sink_ || (samples_==delivery.samples.size()&&presentations_==ready
            &&events_==delivery.events.size())) {return;}
        NativeSessionDelivery chunk;chunk.firstSample=delivery.firstSample+samples_;
        chunk.samples.assign(delivery.samples.begin()+samples_,delivery.samples.end());
        chunk.presentations.assign(delivery.presentations.begin()+presentations_,delivery.presentations.begin()+ready);
        chunk.events.assign(delivery.events.begin()+events_,delivery.events.end());
        sink_(chunk);samples_=delivery.samples.size();presentations_=ready;events_=delivery.events.size();
    }
private:
    const NativeDeliverySink& sink_;
    std::size_t samples_=0,presentations_=0,events_=0;
};
}
