// This file is part of the actsvg packge.
//
// Copyright (C) 2022 CERN for the benefit of the ACTS project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "actsvg/core.hpp"
#include "actsvg/proto/surface.hpp"

namespace actsvg {

using namespace defaults;

namespace display {

/** Draw the surface with a dedicated view
 *
 * @param id_ the identification of this surface
 * @param s_ the surface type
 * @param v_ the view type
 * @param b_ draw the boolean
 * @param fs_ draw as focus
 * @param sc_ draw at scale
 * @param dt_ draw as template
 */
template <typename surface_type, typename view_type>
svg::object surface(const std::string& id_, const surface_type& s_,
                    const view_type& v_, bool _b = true, bool fs_ = false,
                    bool sc_ = false, bool dt_ = false) {

    svg::object s;

    // If the surface has a template and is it defined
    if (s_._template_object.is_defined()) {

        style::transform draw_transform = s_._transform;
        // No rotation nor shift as template
        if (dt_) {
            draw_transform._tr = {0., 0.};
            draw_transform._rot = {0., 0., 0.};
        }
        // Apply scale or not
        if (not sc_) {
            draw_transform._scale = {1., 1.};
        }

        // Create a surface object from the template
        s = draw::from_template(id_, s_._template_object, s_._fill, s_._stroke,
                                draw_transform);
        return s;
    }

    style::transform draw_transform = fs_ ? style::transform{} : s_._transform;
    draw_transform._scale = s_._transform._scale;

    // Surface directly
    if (s_._type == surface_type::e_disc) {

        if (std::abs(s_._opening[0u] + M_PI) >
                std::numeric_limits<scalar>::epsilon() or
            std::abs(s_._opening[1u] - M_PI) >
                std::numeric_limits<scalar>::epsilon()) {
            auto view_vertices = generators::sector_contour(
                s_._radii[0], s_._radii[1], s_._opening[0], s_._opening[1]);
            s = draw::polygon(id_, view_vertices, s_._fill, s_._stroke,
                              draw_transform);

        } else {
            s = draw::circle(id_, {0., 0.}, s_._radii[1u], s_._fill, s_._stroke,
                             draw_transform);

            // A ring is present
            if (s_._radii[0u]) {

                std::string mask_id = id_ + "_mask";

                auto s_c_ = s_;
                s_c_._radii = {0., s_._radii[1u]};

                svg::object outer_mask =
                    surface(id_ + "_mask_surface_outer", s_c_, v_, false);
                outer_mask._fill = style::fill{true};
                outer_mask._stroke = style::stroke{true};
                outer_mask._attribute_map["fill"] = "white";

                s_c_._radii = {0., s_._radii[0u]};
                svg::object inner_mask =
                    surface(id_ + "_mask_surface_inner", s_c_, v_, false);
                inner_mask._fill = style::fill{true};
                inner_mask._stroke = style::stroke{true};
                inner_mask._attribute_map["fill"] = "black";

                // Create the mask object
                svg::object mask;
                mask._fill = style::fill{true};
                mask._stroke = style::stroke{true};
                mask._id = mask_id;
                mask._tag = "mask";
                mask.add_object(outer_mask);
                mask.add_object(inner_mask);

                // Modify the surface
                s._definitions.push_back(mask);
                s._attribute_map["mask"] = utils::id_to_url(mask_id);
            }
        }

    } else {
        auto view_vertices = v_(s_._vertices);
        s = draw::polygon(id_, view_vertices, s_._fill, s_._stroke,
                          draw_transform);
    }

    if (_b) {
        /// Boolean surfaces
        if (s_._boolean_surface.size() == 1u and
            s_._boolean_operation == surface_type::e_subtraction) {
            std::string mask_id = id_ + "_mask";
            // make a new boolean surface
            svg::object outer_mask =
                surface(id_ + "_mask_surface_outer", s_, v_, false);
            outer_mask._fill = style::fill{true};
            outer_mask._stroke = style::stroke{true};
            outer_mask._attribute_map["fill"] = "white";

            svg::object inner_mask = surface(id_ + "_mask_surface_inner",
                                             s_._boolean_surface[0], v_);
            inner_mask._fill = style::fill{true};
            inner_mask._stroke = style::stroke{true};
            inner_mask._attribute_map["fill"] = "black";

            // Create the mask object
            svg::object mask;
            mask._fill = style::fill{true};
            mask._stroke = s_._stroke;
            mask._id = mask_id;
            mask._tag = "mask";
            mask.add_object(outer_mask);
            mask.add_object(inner_mask);

            // Modify the surface
            s._definitions.push_back(mask);
            s._attribute_map["mask"] = utils::id_to_url(mask_id);
        }
    }

    return s;
}

/** Draw eta lines in a zr view
 *
 * @param id_ the identiier
 * @param zr_ the z range of the detector
 * @param rr_ the r range of the detector
 * @param els_ the stroked eta lines + boolean whether to label
 * @param tr_ a potential transform
 *
 * @return a single object containing the frame
 */
static inline svg::object eta_lines(
    const std::string& id_, scalar zr_, scalar rr_,
    const std::vector<std::tuple<std::vector<scalar>, style::stroke, bool,
                                 style::font>>& els_,
    const style::transform& tr_ = style::transform()) {

    svg::object e;
    e._tag = "g";
    e._id = id_;
    e._transform = tr_;

    auto theta_from_eta = [](scalar eta) -> scalar {
        return static_cast<scalar>(2. * std::atan(std::exp(-eta)));
    };

    scalar theta_cut = std::atan2(rr_, zr_);

    for (auto [iet, elt] : utils::enumerate(els_)) {
        auto stroke = std::get<style::stroke>(elt);
        for (auto [ie, eta] :
             utils::enumerate(std::get<std::vector<scalar>>(elt))) {
            scalar theta = theta_from_eta(eta);
            std::array<scalar, 2> start = {0., 0.};
            std::array<scalar, 2> end;
            if (theta < theta_cut) {
                end = {zr_, static_cast<scalar>(zr_ * std::tan(theta))};
            } else {
                end = {static_cast<scalar>(rr_ * 1 / std::tan(theta)), rr_};
            }
            // Draw the line
            std::string uid = std::to_string(iet) + "_" + std::to_string(ie);
            auto e_line =
                draw::line(id_ + "eta_line_" + uid, start, end, stroke);
            e.add_object(e_line);
            // Label it if told to do so
            if (std::get<bool>(elt)) {
                auto font = std::get<style::font>(elt);
                end[0] +=
                    static_cast<scalar>(std::cos(theta) * 0.5 * font._size);
                end[1] +=
                    static_cast<scalar>(std::sin(theta) * 0.5 * font._size);
                if (eta == 0.) {
                    end[0] -= static_cast<scalar>(0.5 * font._size);
                }
                auto e_text = utils::to_string(eta);
                auto e_label =
                    draw::text(id_ + "eta_label_" + uid, end, {e_text}, font);
                e.add_object(e_label);
            }
        }
    }
    return e;
}

}  // namespace display

}  // namespace actsvg
