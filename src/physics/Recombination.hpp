/**
 * @file
 * @brief Definition of charge carrier recombination models
 *
 * @copyright Copyright (c) 2021-2023 CERN and the Allpix Squared authors.
 * This software is distributed under the terms of the MIT License, copied verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 * SPDX-License-Identifier: MIT
 */

#ifndef ALLPIX_RECOMBINATION_MODELS_H
#define ALLPIX_RECOMBINATION_MODELS_H

#include "exceptions.h"

#include "core/config/Configuration.hpp"
#include "core/utils/log.h"
#include "core/utils/unit.h"
#include "objects/SensorCharge.hpp"

namespace allpix {

    /**
     * @ingroup Models
     * @brief Charge carrier recombination models
     */
    class RecombinationModel {
    public:
        /**
         * Default constructor
         */
        RecombinationModel() = default;

        /**
         * Default virtual destructor
         */
        virtual ~RecombinationModel() = default;

        /**
         * Function call operator to obtain recombination status (recombined/alive) for the given carrier and doping
         * concentration
         * @param type Type of charge carrier (electron or hole)
         * @param doping (Effective) doping concentration
         * @param survival_prob Current survival probability for this charge carrier
         * @param timestep Current time step performed for the charge carrier
         * @return Recombination status, true if charge carrier has recombined, false if it still is alive
         */
        virtual bool operator()(const CarrierType& type, double doping, double survival_prob, double timestep) const = 0;
    };

    /**
     * @ingroup Models
     * @brief No recombination
     *
     */
    class None : virtual public RecombinationModel {
    public:
        bool operator()(const CarrierType&, double, double, double) const override { return false; };
    };

    /**
     * @ingroup Models
     * @brief Shockley-Read-Hall recombination of charge carriers in silicon
     *
     * Reference lifetime and doping concentrations, taken from:
     *  - https://doi.org/10.1016/0038-1101(82)90203-9
     *  - https://doi.org/10.1016/0038-1101(76)90022-8
     *
     * Lifetime temperature scaling taken from https://doi.org/10.1016/0038-1101(92)90184-E, Eq. 56 on page 1594
     */
    class ShockleyReadHall : virtual public RecombinationModel {
    public:
        ShockleyReadHall(double temperature, bool doping)
            : electron_lifetime_reference_(Units::get(1e-5, "s")), electron_doping_reference_(Units::get(1e16, "/cm/cm/cm")),
              hole_lifetime_reference_(Units::get(4.0e-4, "s")), hole_doping_reference_(Units::get(7.1e15, "/cm/cm/cm")),
              temperature_scaling_(std::pow(300 / temperature, 1.5)) {
            if(!doping) {
                throw ModelUnsuitable("No doping profile available");
            }
        }

        bool operator()(const CarrierType& type, double doping, double survival_prob, double timestep) const override {
            return survival_prob < (1 - std::exp(-1. * timestep / lifetime(type, doping)));
        };

    protected:
        double lifetime(const CarrierType& type, double doping) const {
            return (type == CarrierType::ELECTRON ? electron_lifetime_reference_ : hole_lifetime_reference_) /
                   (1 + std::fabs(doping) /
                            (type == CarrierType::ELECTRON ? electron_doping_reference_ : hole_doping_reference_)) *
                   temperature_scaling_;
        }

    private:
        double electron_lifetime_reference_;
        double electron_doping_reference_;
        double hole_lifetime_reference_;
        double hole_doping_reference_;
        double temperature_scaling_;
    };

    /**
     * @ingroup Models
     * @brief Auger recombination of charge carriers in silicon
     *
     * Auger coefficient from https://aip.scitation.org/doi/10.1063/1.89694
     *
     */
    class Auger : virtual public RecombinationModel {
    public:
        explicit Auger(bool doping) : auger_coefficient_(Units::get(3.8e-31, "cm*cm*cm*cm*cm*cm*/s")) {
            if(!doping) {
                throw ModelUnsuitable("No doping profile available");
            }
        }

        bool operator()(const CarrierType& type, double doping, double survival_prob, double timestep) const override {
            // Auger only applies to minority charge carriers, if we have a majority carrier always return false (alive):
            auto minorityType = (doping > 0 ? CarrierType::HOLE : CarrierType::ELECTRON);
            return (minorityType != type ? false
                                         : (survival_prob < (1 - std::exp(-1. * timestep / lifetime(type, doping)))));
        };

    protected:
        double lifetime(const CarrierType&, double doping) const { return 1. / (auger_coefficient_ * doping * doping); }

    private:
        double auger_coefficient_;
    };

    /**
     * @ingroup Models
     * @brief Auger recombination of charge carriers in silicon
     *
     */
    class ShockleyReadHallAuger : public ShockleyReadHall, public Auger {
    public:
        ShockleyReadHallAuger(double temperature, bool doping) : ShockleyReadHall(temperature, doping), Auger(doping) {}

        bool operator()(const CarrierType& type, double doping, double survival_prob, double timestep) const override {
            auto minorityType = (doping > 0 ? CarrierType::HOLE : CarrierType::ELECTRON);
            if(minorityType != type) {
                // Auger only applies to minority charge carriers, if we have a majority carrier just return SRH lifetime:
                return ShockleyReadHall::operator()(type, doping, survival_prob, timestep);
            } else {
                // If we have a minority charge carrier, combine the lifetimes:
                auto combined_lifetime =
                    1. / (1. / ShockleyReadHall::lifetime(type, doping) + 1. / Auger::lifetime(type, doping));
                return survival_prob < (1 - std::exp(-1. * timestep / combined_lifetime));
            }
        };
    };

    /**
     * @ingroup Models
     * @brief Simple recombination of charge carriers through constant lifetimes of holes and electrons
     */
    class ConstantLifetime : virtual public RecombinationModel {
    public:
        ConstantLifetime(double electron_lifetime, double hole_lifetime)
            : electron_lifetime_(electron_lifetime), hole_lifetime_(hole_lifetime) {}

        bool operator()(const CarrierType& type, double, double survival_prob, double timestep) const override {
            return survival_prob <
                   (1 - std::exp(-1. * timestep / (type == CarrierType::ELECTRON ? electron_lifetime_ : hole_lifetime_)));
        };

    private:
        double electron_lifetime_;
        double hole_lifetime_;
    };

    /**
     * @brief Wrapper class and factory for recombination models.
     *
     * This class allows to store recombination objects independently of the model chosen and simplifies access to the
     * function call operator. The constructor acts as factory, generating model objects from the model name provided, e.g.
     * from a configuration file.
     */
    class Recombination {
    public:
        /**
         * Default constructor
         */
        Recombination() = default;

        /**
         * Recombination constructor
         * @param config      Configuration of the calling module
         * @param doping      Boolean to indicate presence of doping profile information
         */
        explicit Recombination(const Configuration& config, bool doping = false) {
            try {
                auto model = config.get<std::string>("recombination_model");
                auto temperature = config.get<double>("temperature");
                if(model == "srh") {
                    model_ = std::make_unique<ShockleyReadHall>(temperature, doping);
                } else if(model == "auger") {
                    model_ = std::make_unique<Auger>(doping);
                } else if(model == "combined" || model == "srh_auger") {
                    model_ = std::make_unique<ShockleyReadHallAuger>(temperature, doping);
                } else if(model == "constant") {
                    model_ = std::make_unique<ConstantLifetime>(config.get<double>("lifetime_electron"),
                                                                config.get<double>("lifetime_hole"));
                } else if(model == "none") {
                    LOG(INFO) << "No charge carrier recombination model chosen, finite lifetime not simulated";
                    model_ = std::make_unique<None>();
                } else {
                    throw InvalidModelError(model);
                }
                LOG(INFO) << "Selected recombination model \"" << model << "\"";
            } catch(const ModelError& e) {
                throw InvalidValueError(config, "recombination_model", e.what());
            }
        }

        /**
         * Function call operator forwarded to the mobility model
         * @return Recombination value
         */
        template <class... ARGS> bool operator()(ARGS&&... args) const {
            return model_->operator()(std::forward<ARGS>(args)...);
        }

    private:
        std::unique_ptr<RecombinationModel> model_{};
    };

} // namespace allpix

#endif
