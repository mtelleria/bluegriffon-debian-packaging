// |reftest| skip -- bogus perf test (bug 540512)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Boris Zbarsky
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//-----------------------------------------------------------------------------
var BUGNUMBER = 451673;
var summary = 'TM: Tracing prime number generation';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function doTest(enablejit)
  {
    if (enablejit)
      jit(true);
    else
      jit(false);

    var n = 1000000;
    var start = new Date();
    var i=0;
    var j=0;
    var numprimes=0;
    var limit=0;
    numprimes = 1; // 2 is prime
    var mceil = Math.floor;
    var msqrt = Math.sqrt;
    var isPrime = 1;

    for (i = 3; i<= n; i+=2)
    {
	    isPrime=1;
	    limit = mceil(msqrt(i)+1) + 1;

	    for (j = 3; j < limit; j+=2)
      {
		    if (i % j == 0)
        {
			    isPrime = 0;
			    break;
        }
      }

	    if (isPrime)
      {
		    numprimes ++;
      }
    }

    var end = new Date();

    var timetaken = end - start;
    timetaken = timetaken / 1000;

    if (enablejit)
      jit(false);

    print((enablejit ? '    JIT' : 'Non-JIT') + ": Number of primes up to: " + n + " is " + numprimes + ", counted in " + timetaken + " secs.");

    return timetaken;
  }

  var timenonjit = doTest(false);
  var timejit    = doTest(true);

  expect = true;
  actual = timejit < timenonjit;

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}