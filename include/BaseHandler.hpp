#ifndef BASE_HANDLER_HPP_
#define BASE_HANDLER_HPP_

namespace ntt
{
    class BaseHandler
    {
        public:
            virtual void handle() const = 0;
            virtual ~BaseHandler() = default;
    };
} // namespace ntt

#endif // BASE_HANDLER_HPP_