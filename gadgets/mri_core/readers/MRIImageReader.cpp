#include "MRIImageReader.h"
#include <ismrmrd/ismrmrd.h>
#include <ismrmrd/serialize.h>
#include <ismrmrd/meta.h>
#include "io/primitives.h"
#include "MessageID.h"

namespace Gadgetron {

    namespace {
        template<class T>
        Core::Message
        combine_and_read(ISMRMRD::ImageHeader header,
                         Core::optional<ISMRMRD::MetaContainer> meta,
                         void *data) {

            using namespace Gadgetron::Core;
            auto array = hoNDArray<T>(
                    header.matrix_size[0],
                    header.matrix_size[1],
                    header.matrix_size[2],
                    header.channels,
                    reinterpret_cast<T*>(data),
                    true
            );

            return Core::Message(std::move(header), std::move(array), std::move(meta));
        }

        using ISMRMRD_TYPES = Core::variant<uint16_t, int16_t, uint32_t, int32_t, float, double, std::complex<float>, std::complex<double>>;
        static const auto ismrmrd_type_map = std::unordered_map<uint16_t, ISMRMRD_TYPES>{
                {ISMRMRD::ISMRMRD_USHORT,   uint16_t()},
                {ISMRMRD::ISMRMRD_SHORT,    int16_t()},
                {ISMRMRD::ISMRMRD_INT,      int32_t()},
                {ISMRMRD::ISMRMRD_UINT,     uint32_t()},
                {ISMRMRD::ISMRMRD_FLOAT,    float()},
                {ISMRMRD::ISMRMRD_DOUBLE,   double()},
                {ISMRMRD::ISMRMRD_CXFLOAT,  std::complex<float>()},
                {ISMRMRD::ISMRMRD_CXDOUBLE, std::complex<double>()}
        };
    }

    Core::Message MRIImageReader::read(std::istream &stream) {
        using namespace Gadgetron::Core;

        ISMRMRD::ISMRMRD_Image image;
        ISMRMRD::CompressiblePortableBinaryInputArchive iarchive(stream);
        iarchive(image);

        ISMRMRD::ImageHeader header(image.head);

        optional<ISMRMRD::MetaContainer> meta;
        if (image.head.attribute_string_len > 0) {
            meta = ISMRMRD::MetaContainer();

            ISMRMRD::deserialize(image.attribute_string, *meta);
        }

        // Steal the image data
        void *pData = image.data;
        image.data = nullptr;

        return Core::visit([&](auto type_tag) {
            return combine_and_read<decltype(type_tag)>(std::move(header), std::move(meta), pData);
        }, ismrmrd_type_map.at(header.data_type));
    }

    uint16_t MRIImageReader::slot() {
        return Core::MessageID::GADGET_MESSAGE_ISMRMRD_IMAGE;
    }

    GADGETRON_READER_EXPORT(MRIImageReader)
}
