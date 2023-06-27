#pragma once

#include "StringAdapter.h"
#include "User.h"
#include "WebView.h"
#include <memory>

namespace hestia {
class UserService;
class UserJsonAdapter;

class HestiaUserAuthView : public WebView {
  public:
    HestiaUserAuthView(UserService* user_service);

    ~HestiaUserAuthView();

    HttpResponse::Ptr on_post(
        const HttpRequest& request, const User& user) override;

  private:
    UserService* m_user_service{nullptr};
    std::unique_ptr<JsonAdapter<User>> m_user_adapter;
};
}  // namespace hestia