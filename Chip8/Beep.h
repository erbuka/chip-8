#pragma once

#include <memory>
#include <functional>


namespace c8
{
	class Beep
	{
	public:
		using VolumeFn = std::function<float()>;

		Beep();
		~Beep();

		void Start(const VolumeFn& pullFn);


	private:
		struct Impl;
		std::unique_ptr<Impl> m_Impl;
	};
}