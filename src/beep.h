#pragma once

#include <memory>
#include <functional>


namespace c8
{
	class beep
	{
	public:
		using volume_fn = std::function<float()>;

		beep();
		~beep();

		void start(const volume_fn& pull_fn);


	private:
		struct impl;
		std::unique_ptr<impl> m_impl;
	};
}