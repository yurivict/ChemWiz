

// Arguments for neural network-based atomic system energy computation by M.Wen, E.Tadmor

// M.Wen, E.Tadmor, Hybrid neural network potential for multilayer graphene (2019), Phys. Rev. B 100, 195419 (2019)
// (https://sci-hub.tw/10.1103/PhysRevB.100.195419)

// arg: atoms: [[elt1, [pos1...]], [elt2, [pos2...]], ...]

exports.computeArguments = function(a, atoms) {

	// helper functions used in the computation
	var fc = function(r) { // the cutoff function
		var Rc = 5; // Å
		return r<Rc ? 0.5*(Math.cos(Math.PI*r/Rc) + 1) : 0;
	};
	var sq = function(x) {
		return x*x;
	};
	var dist = function(pos1, pos2) {
		return Math.sqrt(sq(pos1[0]-pos2[0]) + sq(pos1[1]-pos2[1]) + sq(pos1[2]-pos2[2]));
	};
	var len = function(vec) {
		return Math.sqrt(sq(vec[0]) + sq(vec[1]) + sq(vec[2]));
	};
	var dot = function(vec1, vec2) {
		return vec1[0]*vec2[0] + vec1[1]*vec2[1] + vec1[2]*vec2[2];
	};
	var atomPos = function(a) {
		return atoms[a][1];
	};
	var angle = function(pos, pos1, pos2) {
		var pos1r = [pos1[0]-pos[0], pos1[1]-pos[1], pos1[2]-pos[2]];
		var pos2r = [pos2[0]-pos[0], pos2[1]-pos[1], pos2[2]-pos[2]];
		return Math.acos(dot(pos1r, pos2r)/(len(pos1r)*len(pos2r)));
	};
	var pwr = function(x, n) {
		var r = x;
		for (var i = 1; i < n; i++)
			r *= x;
		return r;
	};
	var Ga2a = function(a, eta, Rs) {
		var sum = 0;
		for (var b = 0, be = atoms.length; b<be; b++)
			if (b != a) {
				var rab = dist(atomPos(a), atomPos(b));
				sum += Math.exp(-eta*sq(rab-Rs))*fc(rab);
			}
		return sum;
	};
	var Ga4a = function(a, zeta, lambda, eta) {
		var sum = 0;

		for (var b = 0, be = atoms.length; b<be; b++)
			if (b != a)
				for (var g = b+1; g<be; g++)
					if (g != a) {
						var rab = dist(atomPos(a), atomPos(b));
						var rag = dist(atomPos(a), atomPos(g));
						var rbg = dist(atomPos(b), atomPos(g));

						sum += pwr(1+lambda*Math.cos(angle(atomPos(a), atomPos(b), atomPos(g))), zeta)
						       *Math.exp(-eta*(sq(rab) + sq(rag) + sq(rbg)))
						       *fc(rab)*fc(rag)*fc(rbg);
					}

		var Z = 2;
		for (var z = 0; z < zeta; z++)
			Z /= 2;
			
		return Z*sum;
	};

	return [
		// 8 functions from Table I: Hyperparameters used in the radial descriptor Gα²
		Ga2a(a, 0.0001, 0),
		Ga2a(a, 0.001,  0),
		Ga2a(a, 0.02,   0),
		Ga2a(a, 0.035,  0),
		Ga2a(a, 0.06,   0),
		Ga2a(a, 0.1,    0),
		Ga2a(a, 0.2,    0),
		Ga2a(a, 0.4,    0),

		// 43 functions from Table I: Hyperparameters used in the angular descriptor Gα⁴
		Ga4a(a, 1,    -1,    0.0001), // #1
		Ga4a(a, 1,     1,    0.0001), // #2
		Ga4a(a, 2,    -1,    0.0001), // #3
		Ga4a(a, 2,     1,    0.0001), // #4
		Ga4a(a, 1,    -1,    0.0003), // #5
		Ga4a(a, 1,     1,    0.0003), // #6
		Ga4a(a, 2,    -1,    0.0003), // #7
		Ga4a(a, 2,     1,    0.0003), // #8
		Ga4a(a, 1,    -1,    0.0008), // #9
		Ga4a(a, 1,     1,    0.0008), // #10
		Ga4a(a, 2,    -1,    0.0008), // #11
		Ga4a(a, 2,     1,    0.0008), // #12
		Ga4a(a, 1,    -1,    0.0015), // #13
		Ga4a(a, 1,     1,    0.0015), // #14
		Ga4a(a, 2,    -1,    0.0015), // #15
		Ga4a(a, 2,     1,    0.0015), // #16
		Ga4a(a, 4,    -1,    0.0015), // #17
		Ga4a(a, 4,     1,    0.0015), // #18
		Ga4a(a, 16,   -1,    0.0015), // #19
		Ga4a(a, 16,    1,    0.0015), // #20
		Ga4a(a, 1,    -1,    0.0025), // #21
		Ga4a(a, 1,     1,    0.0025), // #22
		Ga4a(a, 2,    -1,    0.0025), // #23
		Ga4a(a, 2,     1,    0.0025), // #24
		Ga4a(a, 4,    -1,    0.0025), // #25
		Ga4a(a, 4,     1,    0.0025), // #26
		Ga4a(a, 16,   -1,    0.0025), // #27
		Ga4a(a, 16,    1,    0.0025), // #28
		Ga4a(a, 1,    -1,    0.0045), // #29
		Ga4a(a, 1,     1,    0.0045), // #30
		Ga4a(a, 2,    -1,    0.0045), // #31
		Ga4a(a, 2,     1,    0.0045), // #32
		Ga4a(a, 4,    -1,    0.0045), // #33
		Ga4a(a, 4,     1,    0.0045), // #34
		Ga4a(a, 16,   -1,    0.0045), // #35
		Ga4a(a, 16,    1,    0.0045), // #36
		Ga4a(a, 1,    -1,    0.08),   // #37
		Ga4a(a, 1,     1,    0.08),   // #38
		Ga4a(a, 2,    -1,    0.08),   // #39
		Ga4a(a, 2,     1,    0.08),   // #40
		Ga4a(a, 4,    -1,    0.08),   // #41
		Ga4a(a, 4,     1,    0.08),   // #42
		Ga4a(a, 16,    1,    0.08)    // #43
	];
};
