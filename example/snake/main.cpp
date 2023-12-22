#include <html-forms.h>

#include <boost/json.hpp>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <list>
#include <mutex>
#include <random>
#include <sstream>
#include <string_view>
#include <thread>
#include <valarray>
#include <vector>

namespace json = boost::json;

typedef std::valarray<int> vec;

std::ostream &operator<<(std::ostream &os, const vec &v) {
  os << '[';
  if (v.size() > 0)
    os << v[0];

  for (std::size_t i = 1; i < v.size(); ++i) {
    os << ", " << v[i];
  }

  os << ']';
  return os;
}

bool vec_eq(const vec &l, const vec &r) {
  if (l.size() != r.size())
    return false;

  for (std::size_t i = 0; i < l.size(); ++i) {
    if (l[i] != r[i])
      return false;
  }

  return true;
}

class game {
  vec up_, down_, left_, right_;
  html_connection con_;
  std::vector<char> in_buf_;

  bool running_;
  vec *velocity_;
  std::list<vec> body_;
  vec fruit_;
  int width_;
  int height_;

  std::random_device rd_;
  std::mt19937 rand_gen_;

public:
  game(html_connection con)
      : con_{con}, width_{40}, height_{30}, running_{true}, up_{0, -1},
        down_{0, 1}, left_{-1, 0}, right_{1, 0}, velocity_{nullptr},
        rand_gen_{rd_()} {
    in_buf_.resize(1024);
  }

  void reset();
  void input_loop();
  void game_loop() noexcept;
  void stop() noexcept;
  bool slither() noexcept;
  void render() noexcept;
  void generate_fruit() noexcept;
};

int main(int argc, char **argv) {
  html_connection con = html_connection_alloc();

  if (!html_connect(con)) {
    std::cerr << "Failed to connect to html socket" << std::endl;
    return 1;
  }

  if (!html_upload_dir(con, "/", DOCROOT_PATH)) {
    std::cerr << "Failed to upload docroot" << std::endl;
    return 1;
  }

  if (!html_navigate(con, "/index.html")) {
    std::cerr << "Failed to navigate to /index.html" << std::endl;
    return 1;
  }

  std::array<char, 32> sync;
  html_recv_js_message(con, sync.data(), sync.size());

  game snake{con};

  snake.reset();

  std::thread game_th{std::bind(&game::game_loop, &snake)};
  try {
    snake.input_loop();
  } catch (const std::exception &ex) {
    std::cerr << "Error in input loop: " << ex.what() << std::endl;
    snake.stop();
  }

  game_th.join();

  html_connection_free(&con);
  return 0;
}

void game::stop() noexcept { running_ = false; }

void game::input_loop() {
  while (true) {
    int msg_size = html_recv_js_message(con_, in_buf_.data(), in_buf_.size());

    if (msg_size < 0) {
      throw std::runtime_error{"Error reading input message"};
    }

    std::string_view msg{in_buf_.data(), static_cast<std::size_t>(msg_size)};

    vec *next_vel;

    if (msg == "up") {
      next_vel = &up_;
    } else if (msg == "down") {
      next_vel = &down_;
    } else if (msg == "left") {
      next_vel = &left_;
    } else if (msg == "right") {
      next_vel = &right_;
    } else {
      std::ostringstream os;
      os << "Invalid input message: " << msg;
      throw std::logic_error{os.str()};
    }

    if (velocity_) {
      // avoid 180deg turn
      if (!vec_eq(*next_vel, -(*velocity_))) {
        velocity_ = next_vel;
      }
    } else {
      velocity_ = next_vel;
    }
  }
}

void game::reset() {
  velocity_ = nullptr;
  vec head{width_ / 2, height_ / 2};
  body_.clear();
  body_.emplace_back(std::move(head));
  body_.emplace_back(body_.front() + down_);
  generate_fruit();
}

void game::game_loop() noexcept {
  render();

  while (running_) {
    if (slither()) {
      const auto &head = body_.front();

      // out of bounds
      if (head[0] < 0 || head[0] >= width_ || head[1] < 0 ||
          head[1] >= height_) {
        reset();
      }

      // eat fruit!
      if (vec_eq(fruit_, head)) {
        generate_fruit();
        const auto &tail = body_.back();
        body_.push_back(tail); // grow
      }

      render();
    }

    std::chrono::milliseconds ms{100};
    std::this_thread::sleep_for(ms);
  }
}

bool game::slither() noexcept {
  auto v = velocity_;
  if (!v) {
    return false;
  }

  auto next = body_.rbegin();
  auto prev = next;
  ++next;

  // happens when growing
  if (vec_eq(*next, *prev)) {
    prev = next;
    ++next;
  }

  while (next != body_.rend()) {
    *prev = *next;

    prev = next;
    ++next;
  }

  body_.front() += *v;
  return true;
}

void game::render() noexcept {
  json::object output;

  // body
  json::array snake;
  for (const auto &elem : body_) {
    json::array segment = {elem[0], elem[1]};
    snake.emplace_back(std::move(segment));
  }

  output["snake"] = std::move(snake);

  // fruit
  output["fruit"] = json::array{fruit_[0], fruit_[1]};

  auto msg = json::serialize(output);
  html_send_js_message(con_, msg.c_str());
}

void game::generate_fruit() noexcept {
  for (std::size_t i = 0; i < 100; ++i) {
    std::uniform_int_distribution<int> rx{0, width_ - 1}, ry{0, height_ - 1};
    fruit_ = vec{rx(rand_gen_), ry(rand_gen_)};

    for (const auto &elem : body_) {
      if (vec_eq(fruit_, elem))
        continue;
    }

    return;
  }
}
