/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#ifndef MIME_TYPE_HPP
#define MIME_TYPE_HPP

#include "asio-pch.hpp"

std::string_view mime_type(const std::string_view &ext);

#endif
