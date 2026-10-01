#include "stdafx.h"
#include "CppUnitTest.h"
#include "../MusicPlayer2/Define.h"
#include "../MusicPlayer2/DrawCommon.h"
#include "../MusicPlayer2/Common.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTest
{		
	TEST_CLASS(UnitTest1)
	{
	public:
        TEST_METHOD(ScrollTextKeepsCadenceAcrossRefreshRates)
        {
            for (int fps : { 20, 60, 144 })
            {
                CDrawCommon::ScrollInfo info;
                info.last_scroll_time = 0;
                int advances = 0;
                for (int frame = 1; frame <= fps * 3; ++frame)
                    if (info.ShouldAdvance(50, frame * 1000ULL / fps))
                        ++advances;
                Assert::AreEqual(60, advances);
            }
            CDrawCommon::ScrollInfo info;
            info.last_scroll_time = 0;
            Assert::IsFalse(info.ShouldAdvance(50, 16));
            Assert::IsTrue(info.ShouldAdvance(50, 50));
            Assert::IsFalse(info.ShouldAdvance(50, 50));
            Assert::IsTrue(info.ShouldAdvance(50, 10000));
            Assert::IsFalse(info.ShouldAdvance(50, 10016));
        }
		
		TEST_METHOD(TestMethod1)
		{
			// TODO: 在此输入测试代码
            CSize size = CSize(20, 30);
            CCommon::SizeZoom(size, 10);
            Assert::IsTrue(size == CSize(6, 10));
		}

	};
}