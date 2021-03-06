#ifndef AUTHSERVICE_FILTER_CHAIN_H
#define AUTHSERVICE_FILTER_CHAIN_H
#include "envoy/service/auth/v2/external_auth.grpc.pb.h"
#include "src/filters/filter.h"
#include "config/config.pb.h"
#include <memory>

namespace authservice {
namespace filters {
/**
 * FilterChain is an object that wraps a Pipe and the criteria for asserting whether a Pipe should process a request.
 */
class FilterChain {
public:
    virtual ~FilterChain() = default;
    /**
     * Name returns a name given to the filter chain for use in debugging anf logging.
     * @return the name of the filter chain.
     */
    virtual const std::string &Name() const = 0;
    /**
     * Matches can be used to identify whether a chain should be used to process a request.
     * @param request the request to match against.
     * @return true if that chain should process a request else false.
     */
    virtual bool Matches(const ::envoy::service::auth::v2::CheckRequest* request) const = 0;
    /**
     * New creates a new filter instance that can be used to process a request.
     * @return a new filter instance.
     */
    virtual std::unique_ptr<Filter> New() = 0;
};

class FilterChainImpl : public FilterChain {
private:
    authservice::config::FilterChain config_;
public:
    explicit FilterChainImpl(authservice::config::FilterChain config);
    const std::string &Name() const override;
    bool Matches(const ::envoy::service::auth::v2::CheckRequest* request) const override;
    std::unique_ptr<Filter> New() override;
};

}  // namespace filters
}  // namespace authservice
#endif //AUTHSERVICE_FILTER_CHAIN_H
