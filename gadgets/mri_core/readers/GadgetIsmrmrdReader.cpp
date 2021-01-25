#include "GadgetIsmrmrdReader.h"
#include "NHLBICompression.h"
#include <ismrmrd/serialize.h>

namespace Gadgetron {

    Core::Message GadgetIsmrmrdAcquisitionMessageReader::read(std::istream &stream) {

        using namespace Core;
        using namespace std::literals;

        ISMRMRD::ISMRMRD_Acquisition acq;
        ISMRMRD::CompressiblePortableBinaryInputArchive iarchive(stream);
        iarchive(acq);

        optional<hoNDArray<float>> trajectory = Core::none;
        if (acq.traj) {
            // Steal the trajectory data
            trajectory = hoNDArray<float>(acq.head.trajectory_dimensions,
                                          acq.head.number_of_samples,
                                          acq.traj,
                                          true);
            acq.traj = nullptr;
        }
        // Steal the acquisition data
        auto data = hoNDArray<std::complex<float>>(acq.head.number_of_samples,
                                                   acq.head.active_channels,
                                                   acq.data,
                                                   true);
        acq.data = nullptr;

        ISMRMRD::AcquisitionHeader header(acq.head);

        return Core::Message(std::move(header),std::move(data),std::move(trajectory));

    }

    uint16_t GadgetIsmrmrdAcquisitionMessageReader::slot() {
        return 1008;
    }

    Core::Message GadgetIsmrmrdWaveformMessageReader::read(std::istream &stream) {
        using namespace Core;
        using namespace std::literals;

        ISMRMRD::Waveform waveform;
        ISMRMRD::CompressiblePortableBinaryInputArchive iarchive(stream);
        iarchive(waveform);
        ISMRMRD::WaveformHeader header(waveform.head);

        auto data = hoNDArray<uint32_t>(header.number_of_samples, header.channels,
              waveform.data, true);
        waveform.data = nullptr;

        return Message(std::move(header), std::move(data));
    }


    uint16_t GadgetIsmrmrdWaveformMessageReader::slot() {
        return 1026;
    }

    GADGETRON_READER_EXPORT(GadgetIsmrmrdAcquisitionMessageReader)

    GADGETRON_READER_EXPORT(GadgetIsmrmrdWaveformMessageReader)

}
