#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <thread>
#include <utility>

namespace misc
{
    // declaration section

    //! Allows periodic trigger of given function.
    class repeater
    {
    public:
        //! Constructor to initialize the instance.
        /*!
         *  @param interval periodic trigger interval
         *  @param execute_if_interrupted flag to indicate whether to execute
         *  the function even if interrupted
         *  @param fn function that takes in no argument and returns void, i.e. () -> ()
         */
        template <class Rep, class Period, class Fn>
        repeater(
            const std::chrono::duration<Rep, Period> &interval,
            const bool execute_if_interrupted,
            Fn &&fn);

        //! Destructor to stop the periodic trigger.
        /*! Automatically calls stop() if stop() has not yet been invoked.
         */
        ~repeater();

        //! Forces an interrupt to refresh the repeater period interval wait.
        void interrupt();

        //! Forces the repeater to permanently stop.
        void stop();

    private:
        std::shared_ptr<std::atomic_bool> is_running_ptr;
        std::shared_ptr<std::atomic_bool> is_interrupted_ptr;
        std::shared_ptr<std::condition_variable> cv_ptr;
        std::thread t;
    };

    namespace details
    {
        template <class Rep, class Period, class Fn>
        class repeater_functor
        {
        public:
            template <class Fnx>
            repeater_functor(
                const std::shared_ptr<std::atomic_bool> &is_running_ptr,
                const std::shared_ptr<std::atomic_bool> &is_interrupted_ptr,
                const std::shared_ptr<std::condition_variable> &cv_ptr,
                const std::chrono::duration<Rep, Period> &interval,
                const bool execute_if_interrupted,
                Fnx &&fn);

            void operator()();

        private:
            std::shared_ptr<std::atomic_bool> is_running_ptr;
            std::shared_ptr<std::atomic_bool> is_interrupted_ptr;
            std::shared_ptr<std::condition_variable> cv_ptr;
            std::chrono::duration<Rep, Period> interval;
            bool execute_if_interrupted;
            Fn fn;
        };
    }

    // implementation section

    namespace details
    {
        template <class Rep, class Period, class Fn>
        template <class Fnx>
        repeater_functor<Rep, Period, Fn>::repeater_functor(
            const std::shared_ptr<std::atomic_bool> &is_running_ptr,
            const std::shared_ptr<std::atomic_bool> &is_interrupted_ptr,
            const std::shared_ptr<std::condition_variable> &cv_ptr,
            const std::chrono::duration<Rep, Period> &interval,
            const bool execute_if_interrupted,
            Fnx &&fn) :

            is_running_ptr(is_running_ptr),
            is_interrupted_ptr(is_interrupted_ptr),
            cv_ptr(cv_ptr),
            interval(interval),
            execute_if_interrupted(execute_if_interrupted),
            fn(std::forward<Fnx>(fn))
        {
        }

        template <class Rep, class Period, class Fn>
        void repeater_functor<Rep, Period, Fn>::operator()()
        {
            std::mutex m;

            while (is_running_ptr->load())
            {
                std::unique_lock<std::mutex> lock(m);

                const auto is_interrupted = cv_ptr->wait_for(
                    lock,
                    interval,
                    [this] { return is_interrupted_ptr->load(); });
                
                if (is_running_ptr->load())
                {
                    // not interrupted => timeout
                    if (!is_interrupted || execute_if_interrupted)
                    {
                        fn();
                    }

                    // need to refresh the interrupt flag
                    is_interrupted_ptr->store(false);
                }
            }
        }
    }

    template <class Rep, class Period, class Fn>
    repeater::repeater(
        const std::chrono::duration<Rep, Period> &interval,
        const bool execute_if_interrupted,
        Fn &&fn) :

        is_running_ptr(std::make_shared<std::atomic_bool>(true)),
        is_interrupted_ptr(std::make_shared<std::atomic_bool>(false)),
        cv_ptr(std::make_shared<std::condition_variable>()),
        t(details::repeater_functor<Rep, Period, Fn>(
            this->is_running_ptr,
            this->is_interrupted_ptr,
            this->cv_ptr,
            interval,
            execute_if_interrupted,
            std::forward<Fn>(fn)))
    {
    }

    repeater::~repeater()
    {
        try
        {
            stop();

            if (t.joinable())
            {
                t.join();
            }
        }
        catch (...)
        {
            // prevents any exception from leaking out of the destructor
        }
    }

    void repeater::interrupt()
    {
        is_interrupted_ptr->store(true);
        cv_ptr->notify_one();
    }

    void repeater::stop()
    {
        is_running_ptr->store(false);
        is_interrupted_ptr->store(true);
        cv_ptr->notify_one();
    }
}
