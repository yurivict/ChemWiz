#!/usr/bin/env chemwiz

//
// app-neural-net-energy.js is a research application which purpose is to aid in research on
// computation of energy of atomic configurations.
// It allows to:
// 1. Generate and keep track of many test configurations
// 2. Generate a set of neural networks that allow to compute the enery very fast
// 3. Compute enery and dynamics of complex molecular systems
//

var sqlString = require("SqlString");

var helpers = {
	getenvz: function(name, deft) { // returns an environment variable for a supplied name, or the default value when the enironment variable isn't set
		return getenv(name)==null ? deft : getenv(name);
	},
	getenvlz: function(name, deft) { // returns a colon-separated list from the environment variable for a supplied name, or the default value when the enironment variable isn't set
		return this.getenvz(name, deft).split(':');
	},
	requirenAndCreate: function(lst) {
		var mm = [];
		lst.forEach(function(name) {
			mm.push(require(name).create());
		});
		return mm;
	}
};

/// computation engine that we use

var Engines = helpers.requirenAndCreate(helpers.getenvlz("ENGINES", "calc-nwchem"));
var cparams = {dir:"/tmp", precision: "0.001"}

/// functions

function usage() {
	throw "Usage: app-neural-net-based-energy-computation.js {db|generate|compute|export} ...\n";
}

function calcEnergy(engine, params, molecule) {
	var time1 = new Date().getTime();
	var energy = engine.calcEnergy(molecule, params)
	var time2 = new Date().getTime();
	return {engine: engine.kind(), energy: energy, precision: params.precision, elapsed: time2-time1, timestamp: time2};
}

var actions = {
	db: {
		// internals
		_dbFileName_: helpers.getenvz("DB", "nn-energy-computations.sqlite"),
		open: function() {
			return require('sqlite3').openDatabase(this._dbFileName_);
		},
		// db ... commands
		create: function() {
			//File.unlink(this._dbFileName_);
			var db = this.open();
			db.run("CREATE TABLE elt_energy(elt TEXT PRIMARY KEY, energy REAL, precision REAL, engine TEXT);");
			db.run("CREATE TABLE energy(id INTEGER PRIMARY KEY AUTOINCREMENT, energy REAL, precision REAL, elapsed INTEGER, timestamp INTEGER, engine TEXT, comment TEXT);");
			db.run("CREATE TABLE xyz(energy_id INTEGER, elt TEXT, x REAL, y REAL, z REAL, FOREIGN KEY(energy_id) REFERENCES energy(id));");
			db.run("CREATE INDEX index_xyz_energy_id ON xyz(energy_id);");
			db.run("CREATE VIEW\n"+
			       "    energy_view\n"+
			       "AS\n"+
			       "SELECT\n"+
			       "    e.*,\n"+
			       "    (e.energy - (SELECT sum(ee.energy) FROM elt_energy ee, xyz WHERE xyz.energy_id=e.id AND ee.elt=xyz.elt)) AS molecule_energy,\n"+
			       "    (SELECT COUNT(*) FROM xyz WHERE xyz.energy_id = e.id) AS num_atoms,\n"+
			       "    (SELECT GROUP_CONCAT(xyz.elt) FROM xyz WHERE xyz.energy_id = e.id) AS formula\n"+
			       "FROM\n"+
			       "    energy e;"
			);
			db.run("CREATE VIEW\n"+
			       "    xyz_neighbor\n"+
			       "AS\n"+
			       "SELECT\n"+
			       "    ctr.energy_id,\n"+
			       "    ctr.elt AS ctr_elt,\n"+
			       "    ctr.x AS ctr_x,\n"+
			       "    ctr.y AS ctr_y,\n"+
			       "    ctr.z AS ctr_z,\n"+
			       "    n.elt AS n_elt,\n"+
			       "    n.x AS n_x,\n"+
			       "    n.y AS n_y,\n"+
			       "    n.z AS n_z,\n"+
			       "    ((n.x-ctr.x)*(n.x-ctr.x)+(n.y-ctr.y)*(n.y-ctr.y)+(n.z-ctr.z)*(n.z-ctr.z)) AS dist2\n"+
			       "FROM\n"+
			       "    xyz ctr,\n"+
			       "    xyz n\n"+
			       "WHERE\n"+
			       "    n.energy_id = ctr.energy_id\n"+
			       "    AND\n"+
			       "    (\n"+
			       "        n.x != ctr.x  \n"+
			       "        OR\n"+
			       "        n.y != ctr.y\n"+
			       "        OR\n"+
			       "        n.z != ctr.z\n"+
			       "     )\n"+
			       "ORDER BY\n"+
			       "     ctr.energy_id,"+
			       "     ctr.elt,"+
			       "     ctr.x,"+
			       "     ctr.y,"+
			       "     ctr.z,"+
			       "     dist2"
			);
			db.run("CREATE VIEW\n"+
			       "    atomic_neighborhood\n"+
			       "AS\n"+
			       "SELECT\n"+
			       "    ctr.energy_id,\n"+
			       "    ctr.elt,\n"+
			       "    ctr.x,\n"+
			       "    ctr.y,\n"+
			       "    ctr.z,\n"+
			       "    GROUP_CONCAT(n.n_elt||'@'||n.dist2) AS neighbors\n"+
			       "FROM\n"+
			       "    xyz ctr,\n"+
			       "    xyz_neighbor n\n"+
			       "WHERE\n"+
			       "    n.energy_id = ctr.energy_id\n"+
			       "    AND\n"+
			       "    n.ctr_x = ctr.x\n"+
			       "    AND\n"+
			       "    n.ctr_y = ctr.y\n"+
			       "    AND\n"+
			       "    n.ctr_z = ctr.z\n"+
			       "GROUP BY\n"+
			       "    ctr.energy_id, ctr.elt, ctr.x, ctr.y, ctr.z"
		       );
			db.close();
		},
		stats: function() {
		},
		// internally-used commands
		insertEnergy: function(db, energy, precision, elapsed, timestamp, engine, molecule, comment) {
			var db = this.open();
			db.run("BEGIN TRANSACTION")
			db.run("INSERT INTO energy(energy,precision,elapsed,timestamp,engine, comment) VALUES ("+energy+", "+precision+", "+elapsed+", "+timestamp+", '"+engine+"', "+sqlString.escape(comment)+");")
			db.run("SELECT MAX(id) FROM energy;", function(opaque,nCols,fldValues,fldNamesValues) {
				var energy_id = fldValues[0];
				for (var a = 0; a < molecule.numAtoms(); a++) {
					var atom = molecule.getAtom(a);
					var atomPos = atom.getPos();
					db.run("INSERT INTO xyz(energy_id,elt,x,y,z) VALUES ("+energy_id+", '"+atom.getElement()+"', "+atomPos[0]+", "+atomPos[1]+", "+atomPos[2]+");")
				}
			});
			db.run("COMMIT")
		}
	},
	generate: {
		// internals
		_fromMoleculeObject_: function(db, molecule, comment) {
			Engines.forEach(function(engine) {
				try {
					var rec = calcEnergy(engine, cparams, molecule);
					actions.db.insertEnergy(db, rec.energy, rec.precision, rec.elapsed, rec.timestamp, rec.engine, molecule, comment);
				} catch (err) {
					print("EXCEPTION("+engine.kind()+"): molecule failed: "+err);
					writeXyzFile(molecule, "FAILED-molecule-"+engine.kind()+".xyz");
				}
			});
		},
		_fromSmiles_: function(db, smi) {
			var molecule = Moleculex.fromSMILES(smi);
			Engines.forEach(function(engine) {
				try {
					var rec = calcEnergy(engine, cparams, molecule);
					actions.db.insertEnergy(db, rec.energy, rec.precision, rec.elapsed, rec.timestamp, rec.engine, molecule, "smiles: "+smi);
				} catch (err) {
					print("EXCEPTION("+engine.kind()+"): smi="+smi+" failed: "+err);
					writeXyzFile(molecule, "FAILED-"+engine.kind()+"-smi-"+smi+".xyz");
				}
			});
		},
		// generate ... commands
		fromSmilesText: function(smiText) {
			var db = actions.db.open();
			this._fromSmiles_(db, smiText);
			db.close();
		},
		fromSmilesFile: function(fileName) {
			var db = actions.db.open();
			var fromSmiles = this._fromSmiles_;
			File.read(fileName).split("\n").forEach(function(smi) {
				if (smi=="")
					return;
				fromSmiles(db, smi);
			});
			db.close();
		},
		randomConfigurationsOfElt: function(elt, numAtoms, numConfs, boxSize, minDist, maxDist) {
			print("randomConfigurationsOfElt: elt="+elt);
			var randCoord = function() {
        			var max = 1000000;
        			return LegacyRng.rand()%max/max * boxSize - boxSize/2;
			}
			var mm = [];
			while (mm.length < numConfs) {
				var molecule = new Molecule;
				for (var a = 0; a < numAtoms; a++) {
					var atom = new Atom(elt, [randCoord(), randCoord(), randCoord()]);
					molecule.addAtom(atom);
				}
				if (minDist <= molecule.minDistBetweenAtoms() && molecule.maxDistBetweenAtoms() <= maxDist) {
					print("adding the configuration with dists in "+molecule.minDistBetweenAtoms()+".."+molecule.maxDistBetweenAtoms());
					mm.push(molecule);
				}
			}
			var fromMoleculeObject = this._fromMoleculeObject_;
			var db = actions.db.open();
			mm.forEach(function(molecule) {
				fromMoleculeObject(db, molecule, "random: box="+boxSize+" min="+minDist+" max="+maxDist);
			});
			db.close();
		},
		crystalCubic: function(elt, len, cnt) {
			var engine = require("calc-nwchem").create();
			var db = actions.db.open();
			var molecule = new Molecule;
			for (var a = 0; a < cnt[0]; a++)
				for (var b = 0; b < cnt[1]; b++)
					for (var c = 0; c < cnt[2]; c++)
						molecule.addAtom(new Atom(elt, [a*len[0], b*len[1], c*len[2]]));
			this._fromMoleculeObject_(db, molecule, "crystal cubic: len="+len+" cnt="+cnt);
			db.close();
		},
		crystalCcp: function(elt, len, cnt) { // cubic close-packed
			var engine = require("calc-nwchem").create();
			var db = actions.db.open();
			var molecule = new Molecule;
			for (var a = 0; a < cnt[0]; a++)
				for (var b = 0; b < cnt[1]; b++)
					for (var c = 0; c < cnt[2]; c++) {
						molecule.addAtom(new Atom(elt, [a*len[0], b*len[1], c*len[2]]));
						molecule.addAtom(new Atom(elt, [a*len[0] + 0,        b*len[1] + len[1]/2, c*len[2] + len[2]/2]));
						molecule.addAtom(new Atom(elt, [a*len[0] + len[0]/2, b*len[1] + 0,        c*len[2] + len[2]/2]));
						molecule.addAtom(new Atom(elt, [a*len[0] + len[0]/2, b*len[1] + len[1]/2, c*len[2] + 0]));
					}
			this._fromMoleculeObject_(db, molecule, "crystal ccp: len="+len+" cnt="+cnt);
			db.close();
		},
		eltEnergies: function() {
			var C = 20;
			var engine = require("calc-nwchem").create();
			var db = actions.db.open();
			["H", "He",
			 "Li", "Be", "B", "C", "N", "O", "F", "Ne",
			 "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar"].forEach(function(elt) {
				var molecule = new Molecule;
				molecule.addAtom(new Atom(elt, [-C, -C, -C]));
				molecule.addAtom(new Atom(elt, [-C, -C, +C]));
				molecule.addAtom(new Atom(elt, [-C, +C, -C]));
				molecule.addAtom(new Atom(elt, [-C, +C, +C]));
				molecule.addAtom(new Atom(elt, [+C, -C, -C]));
				molecule.addAtom(new Atom(elt, [+C, -C, +C]));
				molecule.addAtom(new Atom(elt, [+C, +C, -C]));
				molecule.addAtom(new Atom(elt, [+C, +C, +C]));
				var rec = calcEnergy(engine, cparams, molecule);
				db.run("DELETE FROM elt_energy WHERE elt='"+elt+"';");
				db.run("INSERT INTO elt_energy(elt,energy,precision,engine) VALUES ('"+elt+"', "+(rec.energy/8)+", "+rec.precision+", '"+rec.engine+"');");
				print("energy for "+elt+" is "+(rec.energy/8));
			});
			db.close();
		}
	},
	compute: {
		neighborhoodArguments: function(energyId, cb) {
			var db = actions.db.open();
			var ctrElt = null;
			var ctrCoords = null;
			var lstNeighbors = null;
			var computeArguments = function(ctrCoords, neighbors) {
				var fc = function(r) { // the cutoff function
					var Rc = 10; // Å
					return 0.5*(Math.cos(Math.PI*r/Rc) + 1);
				};
			};
			var compute = function(ctrElt, ctrCoords, neighbors) {
				cb(ctrElt, ctrCoords, computeArguments(ctrCoords, neighbors));
			};
			db.run("SELECT ctr_elt,ctr_x,ctr_y,ctr_z, n_elt,n_x,n_y,n_z,dist2 FROM xyz_neighbor WHERE energy_id="+energyId+";", function(opaque,nCols,fldValues,fldNamesValues) {
				var ctrEltNew = fldValues[0];
				var ctrCoordsNew = [fldValues[1],fldValues[2],fldValues[3]];
				var nElt = fldValues[4];
				var nCoords = [fldValues[5],fldValues[6],fldValues[7],fldValues[8]];

				if (ctrCoords==null || (ctrCoordsNew[0]!=ctrCoords[0] || ctrCoordsNew[1]!=ctrCoords[1] || ctrCoordsNew[2]!=ctrCoords[2])) {
					ctrElt = ctrEltNew;
					ctrCoords = ctrCoordsNew;
					if (lstNeighbors!=null)
						compute(ctrElt, ctrCoords, lstNeighbors);
					lstNeighbors = [[nElt,nCoords]];
				} else {
					lstNeighbors.push([nElt,nCoords]);
				}
				return false; // continue
			});
			if (lstNeighbors!=null)
				compute(ctrElt, ctrCoords, lstNeighbors);
			db.close();
		}
	},
	export: {
		energyAsXyz: function(energyId, fileName) {
			var db = actions.db.open();
			var xyzData = "";
			var numAtoms = 0;
			db.run("SELECT elt,x,y,z FROM xyz WHERE energy_id="+energyId+";", function(opaque,nCols,fldValues,fldNamesValues) {
				var energy_id = fldValues[0];
				xyzData = xyzData + fldValues[0] + " " + fldValues[1] + " " + fldValues[2] + " " + fldValues[3] + "\n";
				numAtoms++;
				return false; // continue
			});
			xyzData = "energy#"+energyId+" exported by app-neural-net-energy.js\n" + xyzData;
			xyzData = numAtoms+"\n" + xyzData;
			db.close();

			// write the file
			File.write(xyzData, fileName);
		}
	}
};


/// MAIN

function main(args) {
	if (args.length < 1)
		usage();

	if (args[0] == "db") {
		if (args.length < 2)
			usage();
		else if (args[1] == "create") {
			actions.db.create();
		} else if (args[1] == "stats")
			actions.db.stats();
		else
			usage();
	} else if (args[0] == "generate") {
		if (args.length >= 2 && args[1] == "from-smiles-text") {
			for (var i = 2; i < args.length; i++)
				actions.generate.fromSmilesText(args[i]);
		} else if (args.length >= 2 && args[1] == "from-smiles-file") {
			for (var i = 2; i < args.length; i++)
				actions.generate.fromSmilesFile(args[i]);
		} else if (args.length==8 && args[1] == "random-configurations-of-elt") {
			actions.generate.randomConfigurationsOfElt(
				args[2]/*elt*/,
				args[3]/*numAtoms*/,
				args[4]/*numConfs*/,
				args[5]/*box-size (Å)*/,
				args[6]/*min-dist (Å)*/,
				args[7]/*max-dist (Å)*/
			);
		} else if (args.length>2 && args[1] == "crystal") {
			if (args.length==10 && args[2] == "cubic") {
				var elt = args[3];
				var a   = args[4];
				var b   = args[5];
				var c   = args[6];
				var an  = args[7];
				var bn  = args[8];
				var cn  = args[9];
				actions.generate.crystalCubic(elt, [a, b, c], [an, bn, cn]);
			} else if (args.length==10 && args[2] == "ccp") {
				var elt = args[3];
				var a   = args[4];
				var b   = args[5];
				var c   = args[6];
				var an  = args[7];
				var bn  = args[8];
				var cn  = args[9];
				actions.generate.crystalCcp(elt, [a, b, c], [an, bn, cn]);
			} else
				usage();
		} else if (args.length==2 && args[1] == "elt-energies") {
			actions.generate.eltEnergies();
		} else
			usage();
	} else if (args[0] == "compute") {
		if (args.length>2 && args[1] == "neighborhood") {
			if (args.length==4 && args[2] == "arguments") {
				// compute neighborhood args energyId
				actions.compute.neighborhoodArguments(args[3]/*energyId*/, function(ctxElt, ctxX, ctxY, ctxZ, argumentSet) {
					print("ctx: "+ctxElt+" x="+ctxX+" y="+ctxY+" z="+ctxZ+" ->args="+JSON.stringify(argumentSet));
				});
			} else
				usage();
		} if (args.length==4 && args[1] == "configuration-arguments-as-json") {
			// requires
			var argsMapper = require("compute-arguments-symmetry-functions-by-Behler+coworkers");
			// args
			var energyId = args[2];
			var jsonFileName = args[3];
			// open db
			var db = actions.db.open();
			// build the energy_id list
			var energyIds = [];
			db.run("SELECT id,molecule_energy FROM energy_view"+(energyId!="all" ? " WHERE id="+energyId : ""), function(opaque,nCols,fldValues,fldNamesValues) {
				energyIds.push([fldValues[0], fldValues[1]]);
				return false; // continue
			});
			// form json
			var json = [];
			var num = 0;
			energyIds.forEach(function(energyIdAndValue) {
				var energyId = energyIdAndValue[0];
				var energyValue = energyIdAndValue[1];
				var atoms = [];
				db.run("SELECT elt,x,y,z FROM xyz WHERE energy_id="+energyId, function(opaque,nCols,fldValues,fldNamesValues) {
					var elt = fldValues[0];
					var x = fldValues[1];
					var y = fldValues[2];
					var z = fldValues[3];
					atoms.push([elt, [x,y,z]]);
					return false; // continue
				});
				var atomArgs = [];
				for (var i = 0; i < atoms.length; i++)
					atomArgs.push(argsMapper.computeArguments(i, atoms));
				json.push([atomArgs, energyValue]);
			});
			// write the file
			File.write(JSON.stringify(json), jsonFileName);
			// close db
			db.close();
		} else
			usage();
	} else if (args[0] == "export") {
		if (args.length < 2) {
			usage();
		} else if (args.length==4 && args[1] == "energy-as-xyz") {
				actions.export.energyAsXyz(args[2], args[3]);
		} else
			usage();
	} else {
		usage();
	}
}

main(scriptArgs);
print("--END--");
