/**
 * Message for a charge at a pixel in the detector
 *
 * FIXME: better name
 *
 * @author Koen Wolters <koen.wolters@cern.ch>
 */

#ifndef ALLPIX_PIXEL_CHARGE_H
#define ALLPIX_PIXEL_CHARGE_H

#include <Math/DisplacementVector2D.h>

#include "core/messenger/Message.hpp"

namespace allpix {
    // object definition
    class PixelCharge {
    public:
        using Pixel = ROOT::Math::DisplacementVector2D<ROOT::Math::Cartesian2D<int>>;

        PixelCharge(Pixel pixel, unsigned int charge);
        virtual ~PixelCharge();

        PixelCharge(const PixelCharge&);
        PixelCharge& operator=(const PixelCharge&);

        PixelCharge::Pixel getPixel() const;
        unsigned int getCharge() const;

    private:
        Pixel pixel_;
        unsigned int charge_;
    };

    // link to the carrying message
    using PixelChargeMessage = Message<PixelCharge>;
} // namespace allpix

#endif