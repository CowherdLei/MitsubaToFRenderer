#include <mitsuba/render/pathlengthsampler.h>
#include <boost/algorithm/string.hpp>

MTS_NAMESPACE_BEGIN

PathLengthSampler::PathLengthSampler(const Properties &props)
	: ConfigurableObject(props){
	std::string modulationType = boost::to_lower_copy(
					props.getString("modulation", "none"));

	m_decompositionMinBound = props.getFloat("minBound", 0.0f);
	m_decompositionMaxBound = props.getFloat("maxBound", 0.0f);
	m_lambda 				= props.getFloat("lambda",1);
	m_phase 				= props.getFloat("phase",0)*M_PI/180;
	m_P						= props.getInteger("P",32);
	m_neighbors				= props.getInteger("neighbors",3);

	if (modulationType == "none") {
		m_modulationType = ENone;
	} else if (modulationType == "sine") {
		m_modulationType = ESine;
	} else if (modulationType == "square") {
		m_modulationType = ESquare;
	} else if (modulationType == "hamiltonian") {
		m_modulationType = EHamiltonian;
	} else if (modulationType == "mseq") {
		m_modulationType = EMSeq;
	} else if (modulationType == "depthselective") {
		m_modulationType = EDepthSelective;
	} else {
		SLog(EError, "The \"modulation\" parameter must be equal to"
			"either \"none\", \"square\", or \"hamiltonian\", or \"mseq\", or \"depthselective\"!");
	}

	m_areaUnderCorrelationGraph = areaUnderCorrelationGraph(1e6); // use a million point numerical approximation
}
PathLengthSampler::PathLengthSampler(Stream *stream, InstanceManager *manager)
	: ConfigurableObject(stream, manager){
	m_decompositionMinBound = stream->readFloat();
	m_decompositionMaxBound = stream->readFloat();
	m_modulationType		= (EModulationType)stream->readUInt();
	m_lambda				= stream->readFloat();
	m_phase					= stream->readFloat();
	m_P						= stream->readUInt();
	m_neighbors				= stream->readUInt();
}

void PathLengthSampler::addChild(const std::string &name, ConfigurableObject *child) {  }

void PathLengthSampler::serialize(Stream *stream, InstanceManager *manager) const {
	ConfigurableObject::serialize(stream, manager);

	stream->writeFloat(m_decompositionMinBound);
	stream->writeFloat(m_decompositionMaxBound);
	stream->writeUInt(m_modulationType);
	stream->writeFloat(m_lambda);
	stream->writeFloat(m_phase);
	stream->writeUInt(m_P);
	stream->writeUInt(m_neighbors);
}

void PathLengthSampler::configure() {  }

PathLengthSampler::~PathLengthSampler() { }

Float PathLengthSampler::correlationFunction(Float t) const {
	switch(m_modulationType){
		case ENone:{
			SLog(EError, "Cannot call correlation function when the modulation type is not defined");
			break;
		}
		case ESine:{
			t = t + m_phase*m_lambda*INV_PI/2;
			return cos(t*2*M_PI/m_lambda);
			break;
		}
		case ESquare:{
			t = t + m_phase*m_lambda*INV_PI/2;
			return 4/m_lambda*(fabs(fmod(t, m_lambda)-m_lambda/2) - m_lambda/4);
			break;
		}
		case EHamiltonian:{
			t = t + m_phase*m_lambda*INV_PI/2;
			t = fmod(t, m_lambda);
			if(t < m_lambda/6){
				return 6*t/m_lambda;
			}else if(t < m_lambda/2 	&& t >= m_lambda/6){
				return 1.0;
			}else if(t < 2*m_lambda/3 	&& t >= m_lambda/2){
				return 1 - (t - m_lambda/2)*6/m_lambda;
			}else{
				return 0;
			}
			break;
		}
		case EMSeq:{
			return mSeq(t, m_phase);
			break;
		}
		case EDepthSelective:{
			Float value = 0;
			for(int i = 0; i < m_neighbors; i++){
				value += mSeq(t, m_phase + i*(2*M_PI)/m_P);
			}
			value -= (float)(m_neighbors-1)/m_P;
			return value;
			break;
		}
		default:
			SLog(EError, "Modulation type is not defined");
	}
	return 0;
}

Float PathLengthSampler::areaUnderCorrelationGraph(int n) const{
	return areaUnderRestrictedCorrelationGraph(m_decompositionMinBound, m_decompositionMaxBound, n);
}

Float PathLengthSampler::samplePathLengthTarget(ref<Sampler> sampler) const{
	int rejects = 0;
	if(m_modulationType == ENone)
		return m_decompositionMinBound+(m_decompositionMaxBound-m_decompositionMinBound)*sampler->nextFloat();
	else{
		while(true){
			Float t = m_decompositionMinBound+(m_decompositionMaxBound-m_decompositionMinBound)*sampler->nextFloat();
			Float r = sampler->nextFloat();
			if(r < fabs(correlationFunction(t))){
				return t;
			}
			rejects++;
			if(rejects > 1e6){
				SLog(EError, "Rejects exceed 1e6.");
			}
		}
	}
}


Float PathLengthSampler::areaUnderRestrictedCorrelationGraph(Float plMin, Float plMax, int n) const{

	switch(m_modulationType){
		case ENone:{
			return plMax - plMin;
			break;
		}
		case ESine:{
			Float thetaMin 	= plMin * 2 * M_PI/m_lambda + m_phase;
			Float AreaMin  	= floor(thetaMin * INV_TWOPI) * 4;
			thetaMin -= floor(thetaMin * INV_TWOPI) * 2 * M_PI;
			if(thetaMin < M_PI/2){
				AreaMin += sin(thetaMin);
			}else if(thetaMin < 3*M_PI/2){
				AreaMin += 2 - sin(thetaMin);
			}else{
				AreaMin += 4 + sin(thetaMin);
			}

			Float thetaMax 	= plMax * 2 * M_PI/m_lambda + m_phase;
			Float AreaMax  	= floor(thetaMax * INV_TWOPI) * 4;
			thetaMax -= floor(thetaMax * INV_TWOPI) * 2 * M_PI;
			if(thetaMax < M_PI/2){
				AreaMax += sin(thetaMax);
			}else if(thetaMax < 3*M_PI/2){
				AreaMax += 2 - sin(thetaMax);
			}else{
				AreaMax += 4 + sin(thetaMax);
			}

			return (m_lambda * INV_TWOPI * (AreaMax - AreaMin));
			break;
		}
		default:
			Float h = (plMax-plMin)/(n-1);
			Float value = 0.5*( fabs(correlationFunction(plMax))+ fabs(correlationFunction(plMin)));
			for(int i=2; i < n; i++){
				value += fabs(correlationFunction( plMin + h*(i-1) ));
			}
			value *= h;
			return value;
	}
	return 0;
}

Float PathLengthSampler::sampleRestrictedPathLengthTarget(Float plMin, Float plMax, ref<Sampler> sampler){
	int rejects = 0;
	if(m_modulationType == ENone){
		m_areaUnderCorrelationGraph = plMax-plMin;
		return plMin+(plMax-plMin)*sampler->nextFloat();
	}
	else{
		while(true){
			Float t = plMin+(plMax-plMin)*sampler->nextFloat();
			Float r = sampler->nextFloat();
			if(r < fabs(correlationFunction(t))){
				m_areaUnderCorrelationGraph = areaUnderRestrictedCorrelationGraph(plMin, plMax, 1e6);
				return t;
			}
			rejects++;
			if(rejects > 1e6){
				SLog(EError, "Rejects exceed 1e6.");
			}
		}
	}
}

MTS_IMPLEMENT_CLASS(PathLengthSampler, true, ConfigurableObject)

MTS_NAMESPACE_END
